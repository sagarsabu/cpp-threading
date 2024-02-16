#include <chrono>
#include <algorithm>

#include "log/logger.hpp"
#include "threading/events.hpp"
#include "threading/manager_thread.hpp"
#include "threading/worker_thread.hpp"

namespace Sage::Threading
{

enum ManagerTimerEvent
{
    TransmitWork
};

ManagerThread::ManagerThread() :
    Thread{ "MngrThread" }
{ }

void ManagerThread::AttachWorker(Thread* worker)
{
    std::lock_guard lock{ m_workersMtx };
    m_workers.emplace(worker);
}

void  ManagerThread::RequestExit()
{
    Log::Info("exit requested for '%s'", Name());
    m_exitSignal.release();
}

void ManagerThread::WaitForExit()
{
    Log::Info("waiting for exit '%s' request ...", Name());
    m_exitSignal.acquire();
    Log::Info("wait-for-exit '%s' triggered ...", Name());

    TransmitEvent(std::make_unique<ManagerTeardownEvent>());
}

void ManagerThread::WaitForShutdown()
{
    Log::Info("waiting for shutdown '%s' request ...", Name());
    m_shutdownSignal.acquire();
    Log::Info("wait-for-shutdown '%s' triggered ...", Name());

    TeardownWorkers();
    TryWaitForWorkersShutdown();

    // Initiate a stop request
    Stop();

    TryWaitForManagerShutdown();
}

void ManagerThread::TryWaitForWorkersShutdown()
{
    Log::Info("%s workers shutdown started", Name());

    auto workerTeardownStart = Clock::now();
    while (WorkersRunning())
    {
        std::this_thread::sleep_for(20ms);
        auto now = Clock::now();
        auto duration = std::chrono::duration_cast<TimeMilliSec>(now - workerTeardownStart);

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

void ManagerThread::TryWaitForManagerShutdown()
{
    Log::Info("%s manager shutdown starting", Name());

    auto managerTeardownStart = Clock::now();
    while (IsRunning())
    {
        std::this_thread::sleep_for(20ms);
        auto now = Clock::now();
        auto duration = std::chrono::duration_cast<TimeMilliSec>(now - managerTeardownStart);

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
    std::lock_guard lock{ m_workersMtx };

    if (m_workersTerminated)
    {
        Log::Warning("%s workers terminated", Name());
        return;
    }

    for (auto worker : m_workers)
    {
        Log::Info("%s sending work to %s", Name(), worker->Name());
        worker->TransmitEvent(std::make_unique<ManagerWorkerTestEvent>(TEST_TIMEOUT));
        Log::Debug("%s completed sending work to %s", Name(), worker->Name());
    }
}

void ManagerThread::TeardownWorkers()
{
    std::lock_guard lock{ m_workersMtx };

    if (m_workersTerminated)
    {
        Log::Critical("%s workers termination has already been requested", Name());
        return;
    }

    m_workersTerminated = true;

    Log::Info("%s stopping transmit timer", Name());
    RemoveTimer(ManagerTimerEvent::TransmitWork);

    Log::Info("%s tearing down all workers", Name());
    for (auto worker : m_workers)
    {
        Log::Info("%s stopping %s", Name(), worker->Name());
        worker->Stop();
    }

    Log::Info("%s stop requested for all workers", Name());
}

void  ManagerThread::RequestShutdown()
{
    Log::Info("%s shutdown requested", Name());
    m_shutdownSignal.release();
}

bool ManagerThread::WorkersRunning()
{
    std::lock_guard lock{ m_workersMtx };

    bool aWorkerIsRunning = std::any_of(
        m_workers.cbegin(),
        m_workers.cend(),
        [](const Thread* worker) { return worker->IsRunning(); }
    );
    return aWorkerIsRunning;
}

void ManagerThread::Starting()
{
    Log::Info("%s starting ...", Name());

    Log::Info("%s setting up periodic timer for self transmitting", Name());

    AddPeriodicTimer(ManagerTimerEvent::TransmitWork, TRANSMIT_PERIOD);
    StartTimer(ManagerTimerEvent::TransmitWork);
}

void ManagerThread::Stopping()
{
    Log::Info("%s stopping ...", Name());
}

void ManagerThread::HandleEvent(UniqueThreadEvent threadEvent)
{
    switch (threadEvent->Receiver())
    {
        case EventReceiver::Timer:
        {
            auto& event = static_cast<TimerEvent&>(*threadEvent);
            switch (event.Type())
            {
                case ManagerTimerEvent::TransmitWork:
                {
                    SendEventsToWorkers();
                    break;
                }

                default:
                {
                    Log::Error("%s handle-event got unkown timer event:%d", Name(), event.Type());
                    break;
                }
            }

            break;
        }

        case EventReceiver::ManagerThread:
        {

            auto& event = static_cast<ManagerEvent&>(*threadEvent);
            switch (event.Type())
            {
                case ManagerEvent::TeardownWorkers:
                {
                    RequestShutdown();
                    break;
                }

                default:
                {
                    Log::Error("%s handle-event got unkown manager event:%d", Name(), event.Type());
                    break;
                }
            }

            break;
        }

        default:
        {
            Log::Error("%s handle-event got event from unexpected receiver:%s",
                Name(), threadEvent->ReceiverName());
            break;
        }
    }
}

} // namespace Sage::Threading
