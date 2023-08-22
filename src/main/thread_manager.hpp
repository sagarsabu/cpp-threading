#pragma once

#include <memory>
#include <set>

#include "thread/thread.hpp"


namespace Sage::Thread
{

class ManagerThread final : public ThreadI
{
public:
    static inline bool EXIT_REQUESTED{ false };
    static inline std::mutex EXIT_REQ_MTX;
    static inline std::condition_variable EXIT_REQ_CND_VAR;

    static inline bool SHUTDOWN_COMPLETED{ false };
    static inline std::mutex SHUTDOWN_COMPLETE_MTX;
    static inline std::condition_variable SHUTDOWN_CND_VAR;

public:
    ManagerThread();

    void AttachWorker(ThreadI* worker);

    void TeardownWorkers();

    // Signal handler for interrupt
    static void SignalHandler(int);

private:
    int Execute() override;

    void SendEventsToWorkers();

    void Stopping() override;

    bool WorkersStillRunning() const;

private:
    std::set<ThreadI*> m_workers;
    std::atomic<bool> m_workersTerminated;
};

} // namespace Sage::Thread
