#pragma once

#include <memory>
#include <set>
#include <atomic>

#include "thread/thread.hpp"

namespace Sage::Thread
{

class ManagerThread final : public ThreadI
{
public:
    static const inline TimerMS TEARDOWN_THRESHOLD{ 1000 };

public:
    ManagerThread();

    void AttachWorker(ThreadI* worker);

    void TeardownWorkers();

    void RequestExit();

    void WaitForExit();

    void WaitUntilShutdown();

private:
    int Execute() override;

    void SendEventsToWorkers();

    void Stopping() override;

    bool WorkersStillRunning() const;

private:
    std::set<ThreadI*> m_workers;
    std::atomic<bool> m_workersTerminated;
    std::atomic<bool> m_exitRequested;
    std::mutex m_exitReqMtx;
    std::condition_variable m_exitReqCndVar;
    std::atomic<bool> m_shutdownComplete;
    std::mutex m_shutdownCompleteMtx;
    std::condition_variable m_shutdownCompleteCndVar;
};

} // namespace Sage::Thread
