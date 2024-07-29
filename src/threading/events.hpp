#pragma once

#include <unordered_map>

namespace Sage::Threading
{

// Event dispatching

enum class EventReceiver
{
    Self, // loop back events
    Timer,
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
            {EventReceiver::Timer,            "Timer"},
            {EventReceiver::ManagerThread,    "ManagerThread"},
            {EventReceiver::WorkerThread,     "WorkerThread"},
        };

        if (auto itr = receiverNameMap.find(m_receiver); itr != receiverNameMap.end())
        {
            return itr->second;
        }

        return "Unknown";
    }

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

    virtual ~SelfEvent() override = default;

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

class TimerEvent final : public ThreadEvent
{
public:
    using EventID = int;

    explicit TimerEvent(EventID timerEvent) :
        ThreadEvent{ EventReceiver::Timer },
        m_timerEvent{ timerEvent }
    { }

    EventID Type() const { return m_timerEvent; }

private:
    const EventID m_timerEvent;
};

} // namespace Sage::Threading

