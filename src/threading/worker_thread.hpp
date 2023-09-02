#pragma once

#include "threading/thread.hpp"
#include "threading/events.hpp"

namespace Sage::Threading
{

// Events

class WorkerEvent : public ThreadEvent
{
public:
    enum Event
    {
        Test
    };

    virtual ~WorkerEvent() = default;

    Event Type() const { return m_event; }

protected:
    explicit WorkerEvent(Event eventType) :
        ThreadEvent{ EventReceiver::WorkerThread },
        m_event{ eventType }
    { }

private:
    const Event m_event;
};

class WorkerTestEvent final : public WorkerEvent
{
public:
    explicit WorkerTestEvent(const TimeMS& timeout) :
        WorkerEvent{ Event::Test },
        m_timeout{ timeout }
    { }

    TimeMS m_timeout;
};

// Worker thread

class WorkerThread final : public Thread
{
public:
    WorkerThread();

private:
    void HandleEvent(UniqueThreadEvent threadEvent) override;

private:
    static inline std::atomic<uint> s_id{ 0 };
};
} // namespace Sage::Threading
