#include <thread>
#include <queue>
#include <memory>
#include <string>
#include <cassert>

#include "log/logger.hpp"
#include "threading/thread.hpp"
#include "threading/events.hpp"
#include "threading/scoped_deadline.hpp"

namespace Sage::Threading
{

Thread::Thread(const std::string& threadName, const TimeMilliSec& handleEventThreshold) :
    m_threadName{ threadName },
    m_thread{ &Thread::Enter, this },
    m_eventQueueMtx{},
    m_eventQueue{},
    m_eventSignal{ 0 },
    m_exitCode{ 0 },
    m_startLatch{ 1 },
    m_running{ false },
    m_stopping{ false },
    m_stopTimer{ nullptr },
    m_timerEvents{},
    m_handleEventThreshold{ handleEventThreshold }
{
    Log::Debug("%s c'tor", Name());
}

Thread::~Thread()
{
    Log::Debug("%s d'tor", Name());
}

void Thread::Start()
{
    Log::Info("%s start requested", Name());
    m_startLatch.count_down();
}

void Thread::Stop()
{
    // Hold the lock so no other events can come in
    std::lock_guard lock{ m_eventQueueMtx };

    Log::Info("%s stop requested", Name());

    if (m_stopping)
    {
        Log::Critical("%s stop requested when already stopping", Name());
        return;
    }

    // Clear anything in the queue so we get a faster exit
    if (not m_eventQueue.empty())
    {
        Log::Warning("%s flushing %ld events", Name(), m_eventQueue.size());
        while (not m_eventQueue.empty())
        {
            m_eventQueue.pop();
        }
        Log::Trace("%s flushed all events", Name());
    }

    m_eventQueue.emplace(std::make_unique<ExitEvent>());
    m_eventSignal.release();

    // Stop anymore events coming in
    m_stopping = true;
}

void Thread::TransmitEvent(UniqueThreadEvent event)
{
    if (not m_stopping)
    {
        std::lock_guard lock{ m_eventQueueMtx };
        m_eventQueue.emplace(std::move(event));
        m_eventSignal.release();
    }
    else
    {
        Log::Critical("%s transmit-event dropped event for receiver:%s",
            Name(), event->ReceiverName());
    }
}

void Thread::AddPeriodicTimer(TimerEvent::EventID timerEventId, TimeMilliSec period)
{
    auto itr = m_timerEvents.find(timerEventId);
    if (itr != m_timerEvents.end())
    {
        Log::Error("%s add-periodic-timer timer-event-id:%d already exists", Name(), timerEventId);
        return;
    }

    m_timerEvents[timerEventId] = std::make_unique<PeriodicTimer>(period, [this, timerEventId]
    {
        TransmitEvent(std::make_unique<TimerEvent>(timerEventId));
    });
}

void Thread::AddFireOnceTimer(TimerEvent::EventID timerEventId, TimeMilliSec delta)
{
    auto itr = m_timerEvents.find(timerEventId);
    if (itr != m_timerEvents.end())
    {
        Log::Error("%s add-fire-once-timer timer-event-id:%d already exists", Name(), timerEventId);
        return;
    }

    m_timerEvents[timerEventId] = std::make_unique<FireOnceTimer>(delta, [this, timerEventId]
    {
        TransmitEvent(std::make_unique<TimerEvent>(timerEventId));
    });
}

void Thread::RemoveTimer(TimerEvent::EventID timerEventId)
{
    auto itr = m_timerEvents.find(timerEventId);
    if (itr == m_timerEvents.end())
    {
        Log::Error("%s remove-timer timer-event-id:%d does not exist", Name(), timerEventId);
        return;
    }

    m_timerEvents.erase(itr);
}

void Thread::StartTimer(TimerEvent::EventID timerEventId) const
{
    auto itr = m_timerEvents.find(timerEventId);
    if (itr != m_timerEvents.end())
    {
        Log::Debug("%s start-timer timer-event-id:%d timer-id:%d", Name(), timerEventId, itr->second->Id());
        itr->second->Start();
    }
    else
    {
        Log::Error("%s start-timer timer-event-id:%d does not exist", Name(), timerEventId);
    }
}

void Thread::StopTimer(TimerEvent::EventID timerEventId) const
{
    auto itr = m_timerEvents.find(timerEventId);
    if (itr != m_timerEvents.end())
    {
        Log::Debug("%s stop-timer timer-event-id:%d timer-id:%d", Name(), timerEventId, itr->second->Id());
        itr->second->Stop();
    }
    else
    {
        Log::Error("%s stop-timer timer-event-id:%d does not exist", Name(), timerEventId);
    }
}


int Thread::Execute()
{
    std::atomic<bool> readyToExit{ false };
    std::stop_token stopToken{ m_thread.get_stop_token() };
    // Will be execute on this thread via exit event handling
    std::stop_callback stopCb(stopToken, [this, &readyToExit]
    {
        Log::Info("%s stop callback triggered", Name());

        readyToExit = true;
        // notify ourselves to wake up
        m_eventSignal.release();
    });

    while (not readyToExit)
    {
        ProcessEvents();
    }

    return 0;
}

void Thread::ProcessEvents(const TimeMilliSec& timeout)
{
    ScopedDeadline processDeadline{ m_threadName + "@ProcessEvents", timeout };
    bool hasEvent = m_eventSignal.try_acquire_for(timeout);
    if (not hasEvent)
    {
        // Timeout
        return;
    }

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

        if (threadEvent == nullptr)
        {
            Log::Error("%s process-events received null event for receiver", Name());
            continue;
        }

        switch (threadEvent->Receiver())
        {
            case EventReceiver::Self:
            {
                ScopedDeadline handleDeadline{ m_threadName + "@ProcessEvents::HandleSelfEvent", m_handleEventThreshold };
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

    if (tooManyEvents)
    {
        // More to do on next loop so notify ourselves
        Log::Warning("%s process-events max events exceeded threshold:%ld events-this-loop:%ld n-received-events:%ld",
            Name(), MAX_EVENTS_PER_LOOP, eventsForThisLoop, eventsQueued);
        m_eventSignal.release();
    }
    else
    {
        Log::Trace("%s process-events n-received-events:%ld", Name(), eventsForThisLoop);
    }
}

void Thread::HandleEvent(UniqueThreadEvent event)
{
    Log::Warning("%s default handle-event discarding event for receiver:%s",
        Name(), event->ReceiverName());
}

void Thread::HandleSelfEvent(UniqueThreadEvent threadEvent)
{
    if (threadEvent->Receiver() != EventReceiver::Self)
    {
        Log::Critical("%s handle-self-event got event from unexpected receiver:%s",
            Name(), threadEvent->ReceiverName());
        return;
    }

    auto& event = static_cast<SelfEvent&>(*threadEvent);
    switch (event.Type())
    {
        case SelfEvent::Exit:
        {
            Log::Info("%s received exit event. requesting stop.", Name());
            // Trigger via timer so we return out of the main processing loop
            m_stopTimer = std::make_unique<FireOnceTimer>(1ms, [this]
            {
                if (m_thread.request_stop())
                {
                    Log::Info("%s stop request has been executed", Name());
                }
                else
                {
                    Log::Critical("%s stop request failed to executed", Name());
                }
            });
            m_stopTimer->Start();
            break;
        }

        default:
        {
            Log::Error("%s handle-event unknown event:%d",
                Name(), event.Type());
            break;
        }
    }
}

void Thread::Enter()
{
    pthread_setname_np(pthread_self(), Name());

    // For for start trigger
    m_startLatch.wait();

    m_running = true;

    Log::Info("%s starting", Name());
    Starting();

    Log::Info("%s executing ", Name());
    m_exitCode = Execute();

    Log::Info("%s stopping", Name());
    Stopping();

    m_running = false;
}

} // namespace Sage::Threading
