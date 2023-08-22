#include <thread>
#include <queue>
#include <memory>
#include <string>

#include "log/logger.hpp"
#include "thread/thread.hpp"
#include "thread/events.hpp"

using namespace std::chrono_literals;


namespace Sage::Thread
{

ThreadI::ThreadI(const std::string& threadName) :
    std::thread{ ThreadI::Enter, this },
    m_threadName{ threadName },
    m_running{ false },
    m_eventQueueMtx{},
    m_eventQueueCndVar{},
    m_eventQueue{},
    m_exitCode{ 0 }
{
    Log::info("%s c'tor", Name());
}


ThreadI::~ThreadI()
{
    Log::info("%s d'tor", Name());

    if (joinable())
    {
        Log::info("%s joining...", Name());
        join();
    }
}


void ThreadI::TransmitEvent(std::unique_ptr<Event> event)
{
    if (m_running)
    {
        std::unique_lock lock{ m_eventQueueMtx };
        m_eventQueue.emplace(std::move(event));
    }

    m_eventQueueCndVar.notify_one();
}


void ThreadI::Shutdown()
{
    FlushEvents();
    Log::info("%s received exit request", Name());
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
            Log::info("%s received timeout", Name());
            continue;
        }

        if (event->Type() == EventT::Exit)
        {
            Log::info("%s received exit event", Name());
            break;
        }

    }

    return 0;
}

// FIXME. This is blocking the transmitter
std::unique_ptr<Event> ThreadI::WaitForEvent(const TimerMS& timeout)
{
    std::unique_lock lock{ m_eventQueueMtx };
    bool hasEvent = m_eventQueueCndVar.wait_for(
        lock,
        timeout,
        [this] { return not m_eventQueue.empty(); }
    );

    // Timeout
    if (not hasEvent)
        return nullptr;

    std::unique_ptr<Event> unhandledEvent{ nullptr };
    size_t nHandledEvents{ 0 };
    while (not m_eventQueue.empty())
    {
        auto event = std::move(m_eventQueue.front());
        m_eventQueue.pop();
        bool eventHandled{ true };

        switch (event->Type())
        {
        case EventT::Exit:
            eventHandled = false;
            break;

        default:
            HandleEvent(std::move(event));
            break;
        }

        if (eventHandled)
        {
            ++nHandledEvents;
        }
        else
        {
            // Push event upwards
            unhandledEvent = std::move(event);
            break;
        }

        // Don't hold other threads from pushing events to this thread
        if (nHandledEvents > MAX_EVENTS_PER_POLL)
        {
            Log::info("%s WaitForEvents max events handled for this poll. threshold:%ld current-queue-size:%ld",
                Name(), MAX_EVENTS_PER_POLL, m_eventQueue.size());
            break;
        }
    }

    Log::info("%s WaitForEvents handled '%ld' events", Name(), nHandledEvents);
    return unhandledEvent;
}


void ThreadI::FlushEvents()
{
    std::unique_lock lock{ m_eventQueueMtx };

    Log::info("%s Flushing %ld events", Name(), m_eventQueue.size());
    while (not m_eventQueue.empty())
    {
        m_eventQueue.pop();
    }
    Log::info("%s Flushed all events", Name());
}


void ThreadI::Enter(ThreadI* thread)
{
    pthread_t self = pthread_self();
    pthread_setname_np(self, thread->Name());

    Log::info("%s Starting", thread->Name());
    thread->Starting();

    thread->m_running = true;

    Log::info("%s Executing ", thread->Name());
    thread->m_exitCode = thread->Execute();

    thread->m_running = false;

    Log::info("%s Stopping", thread->Name());
    thread->Stopping();
}

} // namespace Sage::Thread
