#pragma once

#include <memory>
#include <set>
#include <atomic>
#include <semaphore>

#include "threading/timer.hpp"
#include "threading/thread.hpp"
#include "threading/events.hpp"

namespace Sage::Threading
{

// Manager events

class ManagerEvent : public ThreadEvent
{
public:
    enum Event
    {
        TeardownWorkers
    };

    virtual ~ManagerEvent() = default;

    Event Type() const { return m_event; }

protected:
    explicit ManagerEvent(Event eventType) :
        ThreadEvent{ EventReceiver::ManagerThread },
        m_event{ eventType }
    { }

private:
    const Event m_event;
};

class ManagerTeardownEvent final : public ManagerEvent
{
public:
    ManagerTeardownEvent() :
        ManagerEvent{ Event::TeardownWorkers }
    { }
};

// Manager thread

class ManagerThread final : public Thread
{
public:
    static const inline TimeMilliSec TEARDOWN_THRESHOLD{ 1000ms };
    static const inline TimeMilliSec TEST_TIMEOUT{ 20ms };

public:
    ManagerThread();

    void AttachWorker(Thread* worker);

    void RequestExit();

    void WaitForExit();

    void WaitUntilWorkersShutdown();

    void WaitUntilManagerShutdown();

private:
    void SendEventsToWorkers();

    void TeardownWorkers();

    void RequestShutdown();

    bool WorkersRunning() const;

    void Stopping() override;

    int Execute() override;

    void HandleEvent(UniqueThreadEvent event) override;

private:
    std::set<Thread*> m_workers;
    std::atomic<bool> m_workersTerminated;
    std::binary_semaphore m_exitSignal;
    std::binary_semaphore m_shutdownSignal;
};

} // namespace Sage::Threading
