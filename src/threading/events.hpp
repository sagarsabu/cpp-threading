#pragma once

#include <atomic>
#include <string_view>
#include <sys/types.h>

#include "channel/channel.hpp"

namespace Sage
{

// forward decls

class ThreadEvent;

// Timer dispatching

using TimerEventId = ssize_t;

struct TimerEvent
{
    virtual ~TimerEvent() noexcept = default;

    enum EventType
    {
        Add,
        Update,
        Stop
    };

    virtual EventType Type() const noexcept = 0;

    TimerEventId m_id{ NextId() };

    static TimerEventId NextId()
    {
        static std::atomic<TimerEventId> s_nextId{ 0 };
        TimerEventId id{ s_nextId.fetch_add(1) };

        // treat 0 as disabled
        if (id == 0)
        {
            id = s_nextId.fetch_add(1);
        }

        return id;
    }
};

struct TimerAddEvent : TimerEvent
{
    EventType Type() const noexcept override { return Add; }

    TimeNS m_timeout;
    std::shared_ptr<Channel::Tx<ThreadEvent>> m_tx;
};

struct TimerUpdateEvent : TimerEvent
{
    EventType Type() const noexcept override { return Update; }

    TimeNS m_newTimeout;
    TimerEventId m_timerToUpdate;
};

struct TimerStopEvent : TimerEvent
{
    EventType Type() const noexcept override { return Stop; }

    TimerEventId m_timerToStop;
};

// Event dispatching

enum class EventReceiver
{
    Self, // loop back events
    TimerExpired,
    ManagerThread,
    WorkerThread
};

class ThreadEvent
{
public:
    virtual ~ThreadEvent() = default;

    EventReceiver Receiver() const { return m_receiver; }

    constexpr std::string_view ReceiverName() const noexcept
    {
        switch (m_receiver)
        {
            case EventReceiver::Self:
                return "Self";
            case EventReceiver::TimerExpired:
                return "Timer";
            case EventReceiver::ManagerThread:
                return "ManagerThread";
            case EventReceiver::WorkerThread:
                return "WorkerThread";
            default:
                return "Unknown";
        }
    }

protected:
    explicit ThreadEvent(EventReceiver receiver) : m_receiver{ receiver } {}

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
    explicit SelfEvent(Event eventType) : ThreadEvent{ EventReceiver::Self }, m_event{ eventType } {}

private:
    const Event m_event;
};

class ExitEvent final : public SelfEvent
{
public:
    ExitEvent() : SelfEvent{ Event::Exit } {}
};

class TimerExpiredEvent final : public ThreadEvent
{
public:

    explicit TimerExpiredEvent(TimerEventId timerEvent) :
        ThreadEvent{ EventReceiver::TimerExpired },
        m_timerId{ timerEvent }
    {
    }

    TimerEventId m_timerId;
};

} // namespace Sage
