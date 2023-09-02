#include <chrono>
#include <memory>
#include <algorithm>

#include "thread/events.hpp"
#include "log/logger.hpp"
#include "main/thread_manager.hpp"

using namespace std::chrono_literals;

namespace Sage::Threading
{

ManagerThread::ManagerThread() :
    Thread{ "MngrThread" },
    m_workers{},
    m_workersTerminated{ false },
    m_exitSignal{ 0 },
    m_shutdownSignal{ 0 }
{ }

void ManagerThread::AttachWorker(Thread* worker)
{
    m_workers.emplace(worker);
}

void  ManagerThread::RequestExit()
{
    Log::Info("exit requested for '%s'", Name());
    m_exitSignal.release();
}

void ManagerThread::WaitForExit()
{
    Log::Info("wait-for-exit '%s' request...", Name());
    m_exitSignal.acquire();
    Log::Info("wait-for-exit '%s' triggered...", Name());

    TransmitEvent(std::make_unique<ManagerTeardownEvent>());
}

void ManagerThread::WaitUntilWorkersShutdown()
{
    Log::Info("wait-until-workers-shutdown '%s' requested", Name());
    m_shutdownSignal.acquire();
    Log::Info("wait-until-workers-shutdown '%s' triggered", Name());

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
    if (m_workersTerminated)
    {
        Log::Critical("%s workers termination has already been requested", Name());
        return;
    }

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
        [](const Thread* worker) { return worker->IsRunning(); }
    );
    return aWorkerIsRunning;
}

void ManagerThread::Stopping()
{
    Log::Info("%s stopping...", Name());
}

int ManagerThread::Execute()
{
    int workerFailed{ 0 };
    bool exitRequested{ false };
    while (not exitRequested)
    {
        auto threadEvent = WaitForEvent(50ms);

        // Timeout
        if (threadEvent == nullptr)
        {
            SendEventsToWorkers();
            continue;
        }

        switch (threadEvent->Receiver())
        {
            case EventReceiverT::Default:
            {
                auto& event = static_cast<DefaultEvent&>(*threadEvent);
                switch (event.Type())
                {
                    case DefaultEventT::Exit:
                    {
                        Log::Info("%s received exit event", Name());
                        exitRequested = true;
                        break;
                    }

                    default:
                    {
                        Log::Error("%s execute got event from unkown event %d from default receiver",
                            Name(), static_cast<int>(event.Type()));
                        break;
                    }
                }

                break;
            }


            default:
            {
                Log::Error("%s execute got event for unexpected receiver:%s",
                    Name(), threadEvent->ReceiverName());
                break;
            }
        }
    }

    for (auto worker : m_workers)
    {
        workerFailed |= (worker->ExitCode() != 0);
    }

    return workerFailed;
}

void ManagerThread::HandleEvent(UniqueThreadEvent threadEvent)
{
    if (threadEvent->Receiver() != EventReceiverT::ThreadManager)
    {
        Log::Error("%s handle-event got event for expected receiver:%s",
            Name(), threadEvent->ReceiverName());
        return;
    }

    auto& event = static_cast<ManagerEvent&>(*threadEvent);
    switch (event.Type())
    {
        case ManagerEventT::TeardownWorkers:
        {
            TeardownWorkers();
            RequestShutdown();
            break;
        }

        default:
        {
            Log::Error("%s handle-event got unkown event:%d",
                Name(), static_cast<int>(event.Type()));
            break;
        }
    }
}

} // namespace Sage::Threading
