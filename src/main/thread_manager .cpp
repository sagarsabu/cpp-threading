#include <chrono>
#include <memory>
#include <algorithm>

#include "thread/events.hpp"
#include "log/logger.hpp"
#include "main/thread_manager.hpp"

using namespace std::chrono_literals;

namespace Sage::Thread
{

ManagerThread::ManagerThread() :
    ThreadI{ "MngrThread" },
    m_workers{},
    m_workersTerminated{ false },
    m_exitRequested{ false },
    m_exitReqMtx{},
    m_exitReqCndVar{},
    m_shutdownComplete{ false },
    m_shutdownCompleteMtx{},
    m_shutdownCompleteCndVar{}
{ }

void ManagerThread::AttachWorker(ThreadI* worker)
{
    m_workers.emplace(worker);
}

void ManagerThread::TeardownWorkers()
{
    Log::Info("%s tearing down all workers", Name());

    m_workersTerminated = true;

    for (auto worker : m_workers)
    {
        Log::Info("%s stopping %s", Name(), worker->Name());
        worker->Stop();
    }

    auto teardownStart = std::chrono::high_resolution_clock::now();
    while (WorkersStillRunning())
    {
        std::this_thread::sleep_for(100ms);

        auto now = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - teardownStart);
        if (duration >= TEARDOWN_THRESHOLD)
        {
            Log::Critical("%s tearing down duration:%ld exceeded threshold duration:%ld ms",
                Name(), duration.count(), TEARDOWN_THRESHOLD.count());
            break;
        }

        Log::Info("%s tearing down duration:%ld", Name(), duration.count());
    }
}

void  ManagerThread::RequestExit()
{
    Log::Info("%s exit requested from thread:[0x%s]", Name(), GetThreadId());

    std::unique_lock lock{ m_exitReqMtx };
    m_exitRequested = true;
    m_exitReqCndVar.notify_one();
}

void ManagerThread::WaitForExit()
{
    Log::Info("%s waiting for exit in thread:[0x%s] ...",
        Name(), GetThreadId());

    std::unique_lock lock{ m_exitReqMtx };
    m_exitReqCndVar.wait(lock, [this]()->bool { return m_exitRequested; });
}

void ManagerThread::WaitUntilShutdown()
{
    Log::Info("%s waiting for shutdown in thread:[0x%s]", Name(), GetThreadId());

    std::unique_lock lock{ m_shutdownCompleteMtx };
    m_shutdownCompleteCndVar.wait(lock, [this]()->bool { return m_shutdownComplete; });
}

int ManagerThread::Execute()
{
    int nFailedWorkers{ 0 };

    while (true)
    {
        auto event = WaitForEvent(100ms);
        // Timeout
        if (event == nullptr)
        {
            SendEventsToWorkers();
            continue;
        }

        if (event->Type() == EventT::Exit)
        {
            Log::Info("%s received exit event", Name());
            break;
        }
    }

    for (auto worker : m_workers)
    {
        nFailedWorkers += (worker->ExitCode() != 0);
    }

    return nFailedWorkers;
}

void ManagerThread::SendEventsToWorkers()
{
    if (m_workersTerminated)
    {
        Log::Warning("%s workers terminated", Name());
        return;
    }

    for (auto worker : m_workers)
    {
        Log::Debug("%s sending work to %s", Name(), worker->Name());
        worker->TransmitEvent(std::make_unique<Event>(EventT::Test));
        Log::Debug("%s completed sending work to %s", Name(), worker->Name());
    }
}

void ManagerThread::Stopping()
{
    Log::Info("%s stopping...", Name());

    std::unique_lock lock{ m_shutdownCompleteMtx };
    m_shutdownComplete = true;
    m_shutdownCompleteCndVar.notify_one();
}

bool ManagerThread::WorkersStillRunning() const
{
    bool aWorkerIsRunning = std::any_of(
        m_workers.cbegin(),
        m_workers.cend(),
        [](const ThreadI* worker) { return worker->IsRunning(); }
    );
    return aWorkerIsRunning;
}

} // namespace Sage::Thread
