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
    m_eventQueueCndVar{},
    m_eventQueue{},
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

void ThreadI::TransmitEvent(std::unique_ptr<Event> event)
{
    if (m_running)
    {
        std::scoped_lock lock{ m_eventQueueMtx };
        m_eventQueue.push(std::move(event));
    }

    // Always notify the running thread
    m_eventQueueCndVar.notify_one();
}

void ThreadI::Start()
{
    Log::Info("%s start requested", Name());

    {
        std::scoped_lock lock{ m_threadCreationMtx };
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
    TransmitEvent(std::make_unique<Event>(EventT::Exit));
}

int ThreadI::Execute()
{
    while (true)
    {
        std::unique_ptr<Event> event = WaitForEvent();
        // Timeout
        if (event == nullptr)
        {
            Log::Info("%s received timeout", Name());
            continue;
        }

        if (event->Type() == EventT::Exit)
        {
            Log::Info("%s received exit event", Name());
            break;
        }

    }

    return 0;
}

std::unique_ptr<Event> ThreadI::WaitForEvent(const TimerMS& timeout)
{
    bool hasEvent{ false };
    size_t nEventsToHandle{ 0 };

    {
        // Need a moveable lock
        std::unique_lock lock{ m_eventQueueMtx };
        hasEvent = m_eventQueueCndVar.wait_for(
            lock,
            timeout,
            [this] { return not m_eventQueue.empty(); }
        );
        nEventsToHandle = m_eventQueue.size();
    }

    // Timeout
    if (not hasEvent)
        return nullptr;

    std::unique_ptr<Event> unhandledEvent{ nullptr };
    size_t eventsHandled{ 0 };
    // Don't hold other threads from pushing events to this thread
    for (; eventsHandled < std::min(nEventsToHandle, MAX_EVENTS_PER_POLL); ++eventsHandled)
    {
        std::unique_ptr<Event> event{ nullptr };
        {
            std::scoped_lock lock{ m_eventQueueMtx };
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

    if (eventsHandled >= MAX_EVENTS_PER_POLL)
    {
        Log::Warning("%s wait-for-events max events exceeded threshold:%ld n-handled-events:%ld n-received-events:ld",
            Name(), MAX_EVENTS_PER_POLL, eventsHandled, nEventsToHandle);
    }
    else
    {
        Log::Debug("%s wait-for-events n-received-events:%ld", Name(), eventsHandled);
    }

    return unhandledEvent;
}

void ThreadI::FlushEvents()
{
    std::scoped_lock lock{ m_eventQueueMtx };

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

} // namespace Sage::Thread
