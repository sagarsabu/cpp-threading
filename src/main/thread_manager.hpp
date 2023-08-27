#pragma once

#include <memory>
#include <set>
#include <atomic>
#include <semaphore>

#include "thread/thread.hpp"

namespace Sage::Thread
{

enum ManagerEventT
{
    Test = EventT::ManagerStart + 1,
    TeardownWorkers
};

struct ManagerTestEvent final : public Event
{
    explicit ManagerTestEvent(const Thread::TimeMS& timeout) :
        Event{ ManagerEventT::Test },
        m_timeout{ timeout }
    { }

    Thread::TimeMS m_timeout;
};

struct ManagerTeardownEvent final : public Event
{
    ManagerTeardownEvent() :
        Event{ ManagerEventT::TeardownWorkers }
    { }
};


class ManagerThread final : public ThreadI
{
public:
    static const inline TimeMS TEARDOWN_THRESHOLD{ 1000 };
    static const inline TimeMS TEST_TIMEOUT{ 20 };

public:
    ManagerThread();

    void AttachWorker(ThreadI* worker);

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

    void HandleEvent(ThreadEvent event) override;

private:
    std::set<ThreadI*> m_workers;
    std::atomic<bool> m_workersTerminated;
    std::binary_semaphore m_exitSignal;
    std::binary_semaphore m_shutdownSignal;
};

} // namespace Sage::Thread
