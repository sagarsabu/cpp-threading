#pragma once

#include <unordered_map>

namespace Sage::Threading
{

// Event dispatching

enum class EventReceiver
{
    Self, // loop back events
    ManagerThread,
    WorkerThread
};

class ThreadEvent
{
public:
    virtual ~ThreadEvent() = default;

    EventReceiver Receiver() const { return m_receiver; }

    const char* ReceiverName() const
    {
        static const std::unordered_map<EventReceiver, const char*> receiverNameMap
        {
            {EventReceiver::Self,             "Self"},
            {EventReceiver::ManagerThread,    "ManagerThread"},
            {EventReceiver::WorkerThread,     "WorkerThread"},
        };

        auto itr = receiverNameMap.find(m_receiver);
        if (itr != receiverNameMap.end())
        {
            return itr->second;
        }

        return "Unknown";
    };

protected:
    explicit ThreadEvent(EventReceiver receiver) : m_receiver{ receiver } { }

private:
    const EventReceiver m_receiver;
};

// Events for the looping back to the running thread

class SelfEvent : public ThreadEvent
{
public:
    enum Event
    {
        Exit
    };

    virtual ~SelfEvent() = default;

    Event Type() const { return m_event; }

protected:
    explicit SelfEvent(Event eventType) :
        ThreadEvent{ EventReceiver::Self },
        m_event{ eventType }
    { }

private:
    const Event m_event;
};

class ExitEvent final : public SelfEvent
{
public:
    ExitEvent() :
        SelfEvent{ Event::Exit }
    { }
};


} // namespace Sage::Threading

