#include <thread>
#include <queue>
#include <memory>
#include <string>
#include <cassert>

#include "log/logger.hpp"
#include "thread/thread.hpp"
#include "thread/events.hpp"

using namespace std::chrono_literals;

namespace Sage::Thread
{

const char* GetThreadId()
{
    std::ostringstream os;
    os << std::hex << std::uppercase << std::this_thread::get_id();

    return os.str().c_str();
}

ThreadI::ThreadI(const std::string& threadName) :
    m_threadName{ threadName },
    m_threadCreationMtx{},
    m_thread{},
    m_eventQueueMtx{},
    m_eventQueue{},
    m_eventQueueSmp{ 0 },
    m_running{ false },
    m_exitCode{ 0 }
{
    Log::Debug("%s c'tor", Name());
}

ThreadI::~ThreadI()
{
    Log::Debug("%s d'tor", Name());

    if (m_thread.joinable())
    {
        Log::Debug("%s joining...", Name());
        m_thread.join();
    }
}

void ThreadI::Start()
{
    Log::Info("%s start requested", Name());

    {
        std::lock_guard lock{ m_threadCreationMtx };
        if (m_running)
        {
            Log::Critical("%s started when already running", Name());
            return;
        }

        m_thread = std::thread(&ThreadI::Enter, this);
    }
}

void ThreadI::Stop()
{
    Log::Info("%s stop requested", Name());

    FlushEvents();
    TransmitEvent(std::make_unique<ExitEvent>());
}

void ThreadI::TransmitEvent(ThreadEvent event)
{
    if (m_running)
    {
        std::lock_guard lock{ m_eventQueueMtx };
        m_eventQueue.push(std::move(event));
        m_eventQueueSmp.release();
    }
    else
    {
        Log::Critical("%s transmit-event dropped event:%d", Name(), event->Type());
    }
}

int ThreadI::Execute()
{
    bool exitRequested{ false };
    while (not exitRequested)
    {
        ThreadEvent event = WaitForEvent();
        // Timeout
        if (event == nullptr)
        {
            Log::Debug("%s received timeout", Name());
            continue;
        }

        switch (event->Type())
        {
            case EventT::Exit:
            {
                Log::Info("%s received exit event", Name());
                exitRequested = true;
                break;
            }


            default:
            {
                Log::Error("%s default execute got unkown event:%d", Name(), event->Type());
                break;
            }
        }
    }

    return 0;
}

ThreadEvent ThreadI::WaitForEvent(const TimerMS& timeout)
{
    bool hasEvent = m_eventQueueSmp.try_acquire_for(timeout);
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

    ThreadEvent unhandledEvent{ nullptr };
    // Don't hold other threads from pushing events to this thread
    size_t eventsToHandleThisLoop{ std::min(currentQueueSize, MAX_EVENTS_PER_LOOP) };
    size_t eventsHandled{ 0 };
    while (eventsHandled < eventsToHandleThisLoop)
    {
        ThreadEvent event{ nullptr };
        {
            std::lock_guard lock{ m_eventQueueMtx };
            event = std::move(m_eventQueue.front());
            m_eventQueue.pop();
        }

        bool eventHandled{ false };
        switch (event->Type())
        {
            case EventT::Exit:
                break;

            default:
            {
                HandleEvent(std::move(event));
                eventHandled = true;
                ++eventsHandled;
                break;
            }
        }

        if (not eventHandled)
        {
            // Push event upwards
            unhandledEvent = std::move(event);
            break;
        }
    }

    bool tooManyEvents = currentQueueSize > MAX_EVENTS_PER_LOOP;
    bool hasUnhandledEvent = eventsHandled < eventsToHandleThisLoop;

    if (tooManyEvents or hasUnhandledEvent)
    {
        // More to do on next loop so notify ourselves
        m_eventQueueSmp.release();
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

    return unhandledEvent;
}

void ThreadI::Enter()
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

void ThreadI::FlushEvents()
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

} // namespace Sage::Thread
