#include <thread>
#include <queue>
#include <memory>
#include <string>
#include <cassert>

#include "log/logger.hpp"
#include "threading/thread.hpp"
#include "threading/events.hpp"
#include "threading/scoped_timer.hpp"

namespace Sage::Threading
{

Thread::Thread(const std::string& threadName) :
    m_threadName{ threadName },
    m_threadCreationMtx{},
    m_thread{},
    m_eventQueueMtx{},
    m_eventQueue{},
    m_eventSignal{ 0 },
    m_exitCode{ 0 },
    m_running{ false },
    m_stopping{ false },
    m_stopTimer{ nullptr }
{
    Log::Debug("%s c'tor", Name());
}

Thread::~Thread()
{
    Log::Debug("%s d'tor", Name());
}

void Thread::Start()
{
    std::lock_guard lock{ m_threadCreationMtx };

    Log::Info("%s start requested", Name());

    if (m_running)
    {
        Log::Critical("%s start requested when already starting", Name());
        return;
    }

    m_thread = std::jthread(&Thread::Enter, this);
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
    ScopedTimer timer{ m_threadName + "@ProcessEvents" };
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
                ScopedTimer handleTimer{ m_threadName + "@ProcessEvents::HandleSelfEvent" };
                HandleSelfEvent(std::move(threadEvent));
                break;
            }

            default:
            {
                ScopedTimer handleTimer{ m_threadName + "@ProcessEvents::HandleTimer" };
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
    m_running = true;

    pthread_setname_np(pthread_self(), Name());

    Log::Info("%s starting", Name());
    Starting();

    Log::Info("%s executing ", Name());
    m_exitCode = Execute();

    Log::Info("%s stopping", Name());
    Stopping();

    m_running = false;
}

} // namespace Sage::Threading
