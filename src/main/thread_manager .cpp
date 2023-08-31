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
    m_exitSignal{ 0 },
    m_shutdownSignal{ 0 }
{ }

void ManagerThread::AttachWorker(ThreadI* worker)
{
    m_workers.emplace(worker);
}

void  ManagerThread::RequestExit()
{
    Log::Info("%s exit requested", Name());
    m_exitSignal.release();
}

void ManagerThread::WaitForExit()
{
    Log::Info("%s waiting for exit request...",
        Name());
    m_exitSignal.acquire();
    Log::Info("%s waiting for exit triggered...",
        Name());

    TransmitEvent(std::make_unique<ManagerTeardownEvent>());
}

void ManagerThread::WaitUntilWorkersShutdown()
{
    Log::Info("%s waiting until shutdown requested", Name());

    m_shutdownSignal.acquire();

    Log::Info("%s workers shutdown starting", Name());
    auto teardownStart = std::chrono::high_resolution_clock::now();

    while (WorkersRunning())
    {
        std::this_thread::sleep_for(20ms);
        auto now = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<TimeMS>(now - teardownStart);

        if (duration >= TEARDOWN_THRESHOLD)
        {
            Log::Critical("%s workers shutdown duration:%ld exceeded threshold duration:%ld ms",
                Name(), duration.count(), TEARDOWN_THRESHOLD.count());
            break;
        }
        Log::Info("%s workers shutdown duration:%ld ms", Name(), duration.count());
    }

    Log::Info("%s workers shutdown complete", Name());
}

void ManagerThread::WaitUntilManagerShutdown()
{
    Stop();

    Log::Info("%s manager shutdown starting", Name());
    auto teardownStart = std::chrono::high_resolution_clock::now();

    while (IsRunning())
    {
        std::this_thread::sleep_for(20ms);
        auto now = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<TimeMS>(now - teardownStart);

        if (duration >= TEARDOWN_THRESHOLD)
        {
            Log::Critical("%s manager shutdown duration:%ld exceeded threshold duration:%ld ms",
                Name(), duration.count(), TEARDOWN_THRESHOLD.count());
            break;
        }
        Log::Info("%s manager shutdown duration:%ld ms", Name(), duration.count());
    }

    Log::Info("%s manager shutdown complete", Name());
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
        worker->TransmitEvent(std::make_unique<ManagerTestEvent>(TEST_TIMEOUT));
        Log::Debug("%s completed sending work to %s", Name(), worker->Name());
    }
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

    Log::Info("%s tore down all workers", Name());
}

void  ManagerThread::RequestShutdown()
{
    Log::Info("%s shutdown requested", Name());
    m_shutdownSignal.release();
}

bool ManagerThread::WorkersRunning() const
{
    bool aWorkerIsRunning = std::any_of(
        m_workers.cbegin(),
        m_workers.cend(),
        [](const ThreadI* worker) { return worker->IsRunning(); }
    );
    return aWorkerIsRunning;
}

void ManagerThread::Stopping()
{
    Log::Info("%s stopping...", Name());
}

int ManagerThread::Execute()
{
    int nFailedWorkers{ 0 };
    bool exitRequested{ false };
    while (not exitRequested)
    {
        auto event = WaitForEvent(50ms);

        // Timeout
        if (event == nullptr)
        {
            SendEventsToWorkers();
            continue;
        }

        switch (event->Type())
        {
            case EventT::Exit:
            {
                Log::Info("%s received exit event", Name());
                exitRequested = true;
                break;
            }

            default:
            {
                Log::Error("%s execute got unkown event:%d", Name(), event->Type());
                break;
            }
        }
    }

    for (auto worker : m_workers)
    {
        nFailedWorkers += (worker->ExitCode() != 0);
    }

    return nFailedWorkers;
}

void ManagerThread::HandleEvent(ThreadEvent event)
{
    switch (event->Type())
    {
        case ManagerEventT::TeardownWorkers:
        {
            TeardownWorkers();
            RequestShutdown();
            break;
        }

        default:
        {
            Log::Error("%s handle-event got unkown event:%d", Name(), event->Type());
            break;
        }
    }
}

} // namespace Sage::Thread
