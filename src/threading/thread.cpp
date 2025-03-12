#include "threading/thread.hpp"
#include "log/logger.hpp"
#include "threading/events.hpp"
#include "timers/scoped_deadline.hpp"

namespace Sage::Threading
{

Thread::Thread(const std::string& threadName, const TimeMS& handleEventThreshold) :
    m_threadName{ threadName },
    m_thread{ &Thread::Enter, this },
    m_handleEventThreshold{ handleEventThreshold }
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
    // Hold the lock so no other events can come in
    std::lock_guard lock{ m_eventQueueMtx };

    LOG_INFO("{} stop requested", Name());

    if (m_stopping)
    {
        LOG_CRITICAL("{} stop requested when already stopping", Name());
        return;
    }

    // Clear anything in the queue so we get a faster exit
    if (not m_eventQueue.empty())
    {
        LOG_WARNING("{} flushing {} events", Name(), m_eventQueue.size());
        while (not m_eventQueue.empty())
        {
            m_eventQueue.pop();
        }
        LOG_TRACE("{} flushed all events", Name());
    }

    m_eventQueue.emplace(std::make_unique<ExitEvent>());
    m_eventSignal.release();

    // Stop anymore events coming in
    m_stopping = true;
}

void Thread::TransmitEvent(UniqueThreadEvent event)
{
    if (m_stopping) [[unlikely]]
    {
        LOG_CRITICAL("{} transmit-event dropped event for receiver:{}", Name(), event->ReceiverName());
        return;
    }

    {
        std::lock_guard lock{ m_eventQueueMtx };
        m_eventQueue.emplace(std::move(event));
    }

    m_eventSignal.release();
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

int Thread::Execute()
{
    std::atomic<bool> readyToExit{ false };
    std::stop_token stopToken{ m_thread.get_stop_token() };
    // Will be execute on this thread via exit event handling
    std::stop_callback stopCb(
        stopToken,
        [this, &readyToExit]
        {
            LOG_INFO("{} stop callback triggered", Name());

            readyToExit = true;
            // notify ourselves to wake up
            m_eventSignal.release();
        }
    );

    while (not readyToExit)
    {
        ProcessEvents();
    }

    return 0;
}

void Thread::ProcessEvents()
{
    bool hasEvent{ m_eventSignal.try_acquire_for(PROCESS_EVENTS_WAIT_TIMEOUT) };
    if (not hasEvent)
    {
        // Timeout
        return;
    }

    // Only start the deadline if there are events to process
    ScopedDeadline processDeadline{ m_threadName + "@ProcessEvents", PROCESS_EVENTS_THRESHOLD };
    size_t eventsQueued{ 0 };
    {
        std::lock_guard lock{ m_eventQueueMtx };
        eventsQueued = m_eventQueue.size();
    }

    // Don't hold other threads from pushing events to this thread
    size_t eventsForThisLoop{ std::min(eventsQueued, MAX_EVENTS_PER_LOOP) };
    bool tooManyEvents = eventsQueued > MAX_EVENTS_PER_LOOP;

    for (size_t eventsHandled{ 0 }; eventsHandled < eventsForThisLoop; eventsHandled++)
    {
        UniqueThreadEvent threadEvent{ nullptr };
        {
            // Lock only for the smallest scope so other threads aren't blocked when transmitting
            std::lock_guard lock{ m_eventQueueMtx };
            threadEvent = std::move(m_eventQueue.front());
            m_eventQueue.pop();
        }

        if (threadEvent == nullptr) [[unlikely]]
        {
            LOG_ERROR("{} process-events received null event for receiver", Name());
        }
        else
        {
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
                    ScopedDeadline handleDeadline{ m_threadName + "@ProcessEvents::HandleTimer",
                                                   m_handleEventThreshold };
                    HandleEvent(std::move(threadEvent));
                    break;
                }
            }
        }
    }

    if (tooManyEvents)
    {
        // More to do on next loop so notify ourselves
        LOG_WARNING(
            "{} process-events max events exceeded threshold:{} events-this-loop:{} n-received-events:{}",
            Name(),
            MAX_EVENTS_PER_LOOP,
            eventsForThisLoop,
            eventsQueued
        );
        m_eventSignal.release();
    }
    else
    {
        LOG_TRACE("{} process-events n-received-events:{}", Name(), eventsForThisLoop);
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

void Thread::Enter()
{
    pthread_setname_np(pthread_self(), Name().c_str());

    // For for start trigger
    m_startLatch.wait();

    m_running = true;

    LOG_INFO("{} starting", Name());
    Starting();

    LOG_INFO("{} executing ", Name());
    m_exitCode = Execute();

    LOG_INFO("{} stopping", Name());
    Stopping();

    m_running = false;
}

} // namespace Sage::Threading
