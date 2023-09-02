#pragma once

#include <memory>
#include <set>
#include <atomic>
#include <semaphore>

#include "thread/thread.hpp"

namespace Sage::Threading
{

enum class ManagerEventT
{
    Test,
    TeardownWorkers
};


struct ManagerEvent : public ThreadEvent
{
    virtual ~ManagerEvent() = default;

    ManagerEventT Type() const { return m_event; }

protected:
    explicit ManagerEvent(ManagerEventT eventType) :
        ThreadEvent{ EventReceiverT::ThreadManager },
        m_event{ eventType }
    { }

private:
    const ManagerEventT m_event;
};

struct ManagerTestEvent final : public ManagerEvent
{
    explicit ManagerTestEvent(const TimeMS& timeout) :
        ManagerEvent{ ManagerEventT::Test },
        m_timeout{ timeout }
    { }

    TimeMS m_timeout;
};

struct ManagerTeardownEvent final : public ManagerEvent
{
    ManagerTeardownEvent() :
        ManagerEvent{ ManagerEventT::TeardownWorkers }
    { }
};


class ManagerThread final : public Thread
{
public:
    static const inline TimeMS TEARDOWN_THRESHOLD{ 1000 };
    static const inline TimeMS TEST_TIMEOUT{ 20 };

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
