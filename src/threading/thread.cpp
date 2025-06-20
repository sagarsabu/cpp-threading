#include <algorithm>
#include <atomic>
#include <memory>
#include <utility>

#include "channel/channel.hpp"
#include "log/logger.hpp"
#include "threading/events.hpp"
#include "threading/thread.hpp"
#include "timers/scoped_deadline.hpp"
#include "timers/timer_thread.hpp"

namespace Sage
{

Thread::Thread(
    const std::string& threadName, TimerThread& timerThread, const TimeMS& handleEventThreshold,
    Channel::ChannelPair<ThreadEvent> channel
) :
    m_threadName{ threadName },
    m_tx{ std::move(channel.tx) },
    m_handleEventThreshold{ handleEventThreshold },
    m_timerThread{ timerThread },
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
    LOG_DEBUG("{} stop requested", Name());

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
    LOG_RETURN_IF(m_stopping.load(std::memory_order_relaxed), LOG_CRITICAL);
    m_tx->send(std::move(event));
}

TimerEventId Thread::StartTimer(const std::string& name, const TimeMS& timeout, TimerExpiredCb cb)
{
    TimerEventId eId{ m_timerThread.RequestTimerAdd(timeout, m_tx) };
    m_timers[eId] = { .name = name, .cb = std::move(cb) };
    LOG_DEBUG("{} start-timer timer-event-id:{} timer-name:{}", Name(), eId, name);
    return eId;
}

void Thread::StopTimer(TimerEventId timerEventId)
{
    auto itr = m_timers.find(timerEventId);
    LOG_RETURN_IF(itr == m_timers.end(), LOG_ERROR);

    LOG_DEBUG("{} stop-timer timer-event-id:{} timer-name:{}", Name(), timerEventId, itr->second.name);
    m_timers.erase(itr);

    m_timerThread.RequestTimerStop(timerEventId);
}

int Thread::Execute(std::unique_ptr<Channel::Rx<ThreadEvent>> rx)
{
    std::stop_token stopToken{ m_thread.get_stop_token() };
    // Will be execute on this thread via exit event handling
    std::stop_callback stopCb(
        stopToken,
        [this, &rx]
        {
            LOG_DEBUG("{} stop callback triggered", Name());
            // notify ourselves to wake up
            rx->wakeImmediately();
        }
    );

    while (not stopToken.stop_requested())
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

            case EventReceiver::TimerExpired:
            {
                const auto& e{ static_cast<const TimerExpiredEvent&>(*threadEvent) };
                if (auto itr{ m_timers.find(e.m_timerId) }; itr != m_timers.end())
                {
                    (itr->second.cb)();
                }
                else
                {
                    LOG_WARNING("got timer expiry for unknown timer-id:{}", e.m_timerId);
                }
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
    LOG_RETURN_IF(threadEvent->Receiver() != EventReceiver::Self, LOG_CRITICAL);

    const auto& event = static_cast<const SelfEvent&>(*threadEvent);
    switch (event.Type())
    {
        case SelfEvent::Exit:
        {
            LOG_INFO("{} received exit event. requesting stop.", Name());
            // will invoke the stop token callback and wake up immediately on the next loop
            if (m_thread.request_stop())
            {
                LOG_DEBUG("{} stop request has been executed", Name());
            }
            else
            {
                LOG_CRITICAL("{} stop request failed to executed", Name());
            }
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

    // stop all timers
    for (const auto& [timerId, _] : m_timers)
    {
        m_timerThread.RequestTimerStop(timerId, false);
    }

    m_running = false;
}

} // namespace Sage
