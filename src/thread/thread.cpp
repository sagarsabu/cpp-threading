#include <thread>
#include <queue>
#include <memory>
#include <string>
#include <cassert>

#include "log/logger.hpp"
#include "thread/thread.hpp"
#include "thread/events.hpp"
#include "thread/timer.hpp"

using namespace std::chrono_literals;

namespace Sage::Threading
{

Thread::Thread(const std::string& threadName) :
    m_threadName{ threadName },
    m_thread{},
    m_eventQueueMtx{},
    m_eventQueue{},
    m_eventSignal{ 0 },
    m_started{ false },
    m_running{ false },
    m_stoped{ false },
    m_exitCode{ 0 }
{
    Log::Debug("%s c'tor", Name());
}

Thread::~Thread()
{
    Log::Debug("%s d'tor", Name());

    if (m_thread.joinable())
    {
        // NOTE: Not sure if this is a great idea as it could cause exit to hang...
        Log::Debug("%s joining...", Name());
        m_thread.join();
    }
}

void Thread::Start()
{
    Log::Info("%s start requested", Name());

    if (m_started)
    {
        Log::Critical("%s start requested when already starting", Name());
        return;
    }

    m_started = true;
    m_thread = std::thread(&Thread::Enter, this);
}

void Thread::Stop()
{
    Log::Info("%s stop requested", Name());
    if (m_stoped)
    {
        Log::Critical("%s stop requested when already stopping", Name());
        return;
    }

    m_stoped = true;
    FlushEvents();
    TransmitEvent(std::make_unique<ExitEvent>());
}

void Thread::TransmitEvent(UniqueThreadEvent event)
{
    if (m_running)
    {
        std::lock_guard lock{ m_eventQueueMtx };
        m_eventQueue.push(std::move(event));
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
    bool exitRequested{ false };
    while (not exitRequested)
    {
        UniqueThreadEvent threadEvent = WaitForEvent();
        // Timeout
        if (threadEvent == nullptr)
        {
            continue;
        }

        switch (threadEvent->Receiver())
        {
            case EventReceiverT::Default:
            {
                auto& event = static_cast<DefaultEvent&>(*threadEvent);
                switch (event.Type())
                {
                    case DefaultEventT::Exit:
                    {
                        Log::Info("%s received exit event", Name());
                        exitRequested = true;
                        break;
                    }

                    default:
                    {
                        Log::Error("%s default execute got event from unkown event %d from default receiver",
                            Name(), static_cast<int>(event.Type()));
                        break;
                    }
                }

                break;
            }


            default:
            {
                Log::Error("%s default execute got event from unkown receiver:%s",
                    Name(), threadEvent->ReceiverName());
                break;
            }
        }
    }

    return 0;
}

UniqueThreadEvent Thread::WaitForEvent(const TimeMS& timeout)
{
    ScopeTimer timer{ m_threadName + "@WaitForEvent" };
    bool hasEvent = m_eventSignal.try_acquire_for(timeout);
    if (not hasEvent)
    {
        // Timeout
        return nullptr;
    }

    size_t currentQueueSize{ 0 };
    {
        std::lock_guard lock{ m_eventQueueMtx };
        currentQueueSize = m_eventQueue.size();
    }

    UniqueThreadEvent unhandledThreadEvent{ nullptr };
    // Don't hold other threads from pushing events to this thread
    size_t eventsToHandleThisLoop{ std::min(currentQueueSize, MAX_EVENTS_PER_LOOP) };
    size_t eventsHandled{ 0 };
    while (eventsHandled < eventsToHandleThisLoop)
    {
        UniqueThreadEvent threadEvent{ nullptr };
        {
            std::lock_guard lock{ m_eventQueueMtx };
            threadEvent = std::move(m_eventQueue.front());
            m_eventQueue.pop();
        }

        bool eventHandled{ false };
        switch (threadEvent->Receiver())
        {
            case EventReceiverT::Default:
                break;

            default:
            {
                ScopeTimer handleTimer{ m_threadName + "@WaitForEvent::HandleTimer::" + threadEvent->ReceiverName() };
                HandleEvent(std::move(threadEvent));
                eventHandled = true;
                ++eventsHandled;
                break;
            }
        }

        if (not eventHandled)
        {
            // Push event upwards
            unhandledThreadEvent = std::move(threadEvent);
            break;
        }
    }

    bool tooManyEvents = currentQueueSize > MAX_EVENTS_PER_LOOP;
    bool hasUnhandledEvent = eventsHandled < eventsToHandleThisLoop;

    if (tooManyEvents or hasUnhandledEvent)
    {
        // More to do on next loop so notify ourselves
        m_eventSignal.release();
    }

    if (tooManyEvents)
    {
        Log::Warning("%s wait-for-events max events exceeded threshold:%ld n-handled-events:%ld n-events-this-loop:%ld n-received-events:%ld",
            Name(), MAX_EVENTS_PER_LOOP, eventsHandled, eventsToHandleThisLoop, currentQueueSize);
    }
    else
    {
        Log::Debug("%s wait-for-events n-received-events:%ld", Name(), eventsHandled);
    }

    return unhandledThreadEvent;
}

void Thread::HandleEvent(UniqueThreadEvent event)
{
    Log::Warning("%s default handle-event discarding event for receiver:%s",
        Name(), event->ReceiverName());
}

void Thread::Enter()
{
    pthread_t self = pthread_self();
    pthread_setname_np(self, Name());

    Log::Info("%s starting", Name());
    Starting();

    Log::Info("%s executing ", Name());
    m_running = true;
    m_exitCode = Execute();
    m_running = false;

    Log::Info("%s stopping", Name());
    Stopping();
}

void Thread::FlushEvents()
{
    std::lock_guard lock{ m_eventQueueMtx };

    if (not m_eventQueue.empty())
    {
        Log::Warning("%s flushing %ld events", Name(), m_eventQueue.size());
    }

    while (not m_eventQueue.empty())
    {
        m_eventQueue.pop();
    }

    Log::Info("%s flushed all events", Name());
}

} // namespace Sage::Threading
