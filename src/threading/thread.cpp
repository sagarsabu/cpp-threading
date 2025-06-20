#include "threading/thread.hpp"
#include "channel/channel.hpp"
#include "log/logger.hpp"
#include "threading/events.hpp"
#include "timers/scoped_deadline.hpp"

#include <algorithm>
#include <memory>
#include <utility>

namespace Sage::Threading
{

Thread::Thread(
    const std::string& threadName, const TimeMS& handleEventThreshold, Channel::ChannelPair<ThreadEvent> channel
) :
    m_threadName{ threadName },
    m_tx{ std::move(channel.tx) },
    m_handleEventThreshold{ handleEventThreshold },
    m_thread{ &Thread::Enter, this, std::move(channel.rx) }
{
    LOG_DEBUG("{} c'tor", Name());
}

Thread::~Thread() { LOG_DEBUG("{} d'tor", Name()); }

void Thread::Start()
{
    LOG_INFO("{} start requested", Name());
    m_startLatch.count_down();
}

void Thread::Stop()
{
    LOG_INFO("{} stop requested", Name());

    if (m_stopping)
    {
        LOG_CRITICAL("{} stop requested when already stopping", Name());
        return;
    }

    // Stop anymore events coming in
    m_stopping = true;

    // Clear anything in the queue so we get a faster exit
    m_tx->flushAndSend(std::make_unique<ExitEvent>());
}

void Thread::TransmitEvent(UniqueThreadEvent event)
{
    if (m_stopping) [[unlikely]]
    {
        LOG_CRITICAL("{} transmit-event dropped event for receiver:{}", Name(), event->ReceiverName());
        return;
    }

    m_tx->send(std::move(event));
}

void Thread::AddPeriodicTimer(TimerEvent::EventID timerEventId, TimeNS period)
{
    auto itr = m_timers.find(timerEventId);
    if (itr != m_timers.end())
    {
        LOG_ERROR("{} add-periodic-timer timer-event-id:{} already exists", Name(), timerEventId);
        return;
    }

    const std::string timerName{ m_threadName + "-Periodic-" + std::to_string(timerEventId) };
    m_timers[timerEventId] = std::make_unique<PeriodicTimer>(
        timerName, period, [this, timerEventId] { TransmitEvent(std::make_unique<TimerEvent>(timerEventId)); }
    );
}

// cppcheck-suppress unusedFunction
void Thread::AddFireOnceTimer(TimerEvent::EventID timerEventId, TimeNS delta)
{
    auto itr = m_timers.find(timerEventId);
    if (itr != m_timers.end())
    {
        LOG_ERROR("{} add-fire-once-timer timer-event-id:{} already exists", Name(), timerEventId);
        return;
    }

    const std::string timerName{ m_threadName + "-FireOnce-" + std::to_string(timerEventId) };
    m_timers[timerEventId] = std::make_unique<FireOnceTimer>(
        timerName, delta, [this, timerEventId] { TransmitEvent(std::make_unique<TimerEvent>(timerEventId)); }
    );
}

void Thread::RemoveTimer(TimerEvent::EventID timerEventId)
{
    auto itr = m_timers.find(timerEventId);
    if (itr == m_timers.end())
    {
        LOG_ERROR("{} remove-timer timer-event-id:{} does not exist", Name(), timerEventId);
        return;
    }

    m_timers.erase(itr);
}

void Thread::StartTimer(TimerEvent::EventID timerEventId) const
{
    auto itr = m_timers.find(timerEventId);
    if (itr == m_timers.end())
    {
        LOG_ERROR("{} start-timer timer-event-id:{} does not exist", Name(), timerEventId);
        return;
    }

    LOG_DEBUG("{} start-timer timer-event-id:{} timer-name:{}", Name(), timerEventId, itr->second->Name());
    itr->second->Start();
}

// cppcheck-suppress unusedFunction
void Thread::StopTimer(TimerEvent::EventID timerEventId) const
{
    auto itr = m_timers.find(timerEventId);
    if (itr == m_timers.end())
    {
        LOG_ERROR("{} stop-timer timer-event-id:{} does not exist", Name(), timerEventId);
        return;
    }

    LOG_DEBUG("{} stop-timer timer-event-id:{} timer-name:{}", Name(), timerEventId, itr->second->Name());
    itr->second->Stop();
}

int Thread::Execute(std::unique_ptr<Channel::Rx<ThreadEvent>> rx)
{
    std::atomic<bool> readyToExit{ false };
    std::stop_token stopToken{ m_thread.get_stop_token() };
    // Will be execute on this thread via exit event handling
    std::stop_callback stopCb(
        stopToken,
        [this, &readyToExit, &rx]
        {
            LOG_INFO("{} stop callback triggered", Name());

            readyToExit = true;
            // notify ourselves to wake up
            rx->wakeImmediately();
        }
    );

    while (not readyToExit)
    {
        ProcessEvents(*rx);
    }

    return 0;
}

void Thread::ProcessEvents(Channel::Rx<ThreadEvent>& rx)
{
    auto [events, eventLeftInQueue]{ rx.tryReceiveLimitedMany(PROCESS_EVENTS_WAIT_TIMEOUT, MAX_EVENTS_PER_LOOP) };
    if (events.empty())
    {
        return;
    }

    // Only start the deadline if there are events to process
    ScopedDeadline processDeadline{ m_threadName + "@ProcessEvents", PROCESS_EVENTS_THRESHOLD };

    for (auto& threadEvent : events)
    {
        if (threadEvent == nullptr) [[unlikely]]
        {
            LOG_ERROR("{} process-events received null event for receiver", Name());
            continue;
        }

        switch (threadEvent->Receiver())
        {
            case EventReceiver::Self:
            {
                ScopedDeadline handleDeadline{ m_threadName + "@ProcessEvents::HandleSelfEvent",
                                               m_handleEventThreshold };
                HandleSelfEvent(std::move(threadEvent));
                break;
            }

            default:
            {
                ScopedDeadline handleDeadline{ m_threadName + "@ProcessEvents::HandleTimer", m_handleEventThreshold };
                HandleEvent(std::move(threadEvent));
                break;
            }
        }
    }

    // too many events ?
    if (eventLeftInQueue > 0)
    {
        // More to do on next loop so notify ourselves
        LOG_WARNING(
            "{} process-events max events exceeded threshold:{} n-events-left:{}",
            Name(),
            MAX_EVENTS_PER_LOOP,
            eventLeftInQueue
        );
        rx.wakeImmediately();
    }
    else
    {
        LOG_TRACE("{} process-events n-received-events:{}", Name(), events.size());
    }
}

void Thread::HandleSelfEvent(UniqueThreadEvent threadEvent)
{
    if (threadEvent->Receiver() != EventReceiver::Self) [[unlikely]]
    {
        LOG_CRITICAL("{} handle-self-event got event from unexpected receiver:{}", Name(), threadEvent->ReceiverName());
        return;
    }

    const auto& event = static_cast<const SelfEvent&>(*threadEvent);
    switch (event.Type())
    {
        case SelfEvent::Exit:
        {
            LOG_INFO("{} received exit event. requesting stop.", Name());
            // Trigger via timer so we return out of the main processing loop
            m_stopTimer = std::make_unique<FireOnceTimer>(
                m_threadName + "-ExitTimer",
                1ms,
                [this]
                {
                    if (m_thread.request_stop())
                    {
                        LOG_INFO("{} stop request has been executed", Name());
                    }
                    else
                    {
                        LOG_CRITICAL("{} stop request failed to executed", Name());
                    }
                }
            );
            m_stopTimer->Start();
            break;
        }

        default:
        {
            LOG_ERROR("{} handle-event unknown event:{}", Name(), (int)event.Type());
            break;
        }
    }
}

void Thread::Enter(std::unique_ptr<Channel::Rx<ThreadEvent>> rx)
{
    pthread_setname_np(pthread_self(), Name().c_str());

    // For for start trigger
    m_startLatch.wait();

    m_running = true;

    LOG_INFO("{} starting", Name());
    Starting();

    LOG_INFO("{} executing ", Name());
    m_exitCode = Execute(std ::move(rx));

    LOG_INFO("{} stopping", Name());
    Stopping();

    m_running = false;
}

} // namespace Sage::Threading
