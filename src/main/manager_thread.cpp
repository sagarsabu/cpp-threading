#include <chrono>
#include <algorithm>

#include "log/logger.hpp"
#include "threading/events.hpp"
#include "main/manager_thread.hpp"
#include "main/worker_thread.hpp"

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

void  ManagerThread::RequestShutdown()
{
    LOG_INFO("shutdown requested for '%s'", Name());
    m_shutdownInitiateSignal.release();
}

// Called from main thread
void ManagerThread::WaitForShutdown()
{
    LOG_INFO("waiting for shutdown initiate signal for '%s'", Name());
    m_shutdownInitiateSignal.acquire();
    LOG_INFO("shutdown initiate signal for '%s' acquired", Name());

    TransmitEvent(std::make_unique<ManagerShutdownEvent>());

    LOG_INFO("waiting for shutdown initiated signal for '%s'", Name());
    m_shutdownInitiatedSignal.acquire();
    LOG_INFO("shutdown initiated signal for '%s' acquired", Name());

    // Initiate a stop request for the manager thread
    Stop();

    TryWaitForWorkersShutdown();
    TryWaitForManagerShutdown();
}

void ManagerThread::TryWaitForWorkersShutdown()
{
    LOG_INFO("%s workers shutdown started", Name());

    auto workerTeardownStart = Clock::now();
    while (WorkersRunning())
    {
        std::this_thread::sleep_for(20ms);
        auto now = Clock::now();
        auto duration = std::chrono::duration_cast<TimeMS>(now - workerTeardownStart);

        if (duration >= TEARDOWN_THRESHOLD)
        {
            LOG_CRITICAL("%s workers shutdown duration:%ld exceeded threshold duration:%ld ms",
                Name(), duration.count(), TEARDOWN_THRESHOLD.count());
            break;
        }
        LOG_INFO("%s workers shutdown duration:%ld ms", Name(), duration.count());
    }

    LOG_INFO("%s workers shutdown complete", Name());
}

void ManagerThread::TryWaitForManagerShutdown()
{
    LOG_INFO("%s manager shutdown starting", Name());

    auto managerTeardownStart = Clock::now();
    while (IsRunning())
    {
        std::this_thread::sleep_for(20ms);
        auto now = Clock::now();
        auto duration = std::chrono::duration_cast<TimeMS>(now - managerTeardownStart);

        if (duration >= TEARDOWN_THRESHOLD)
        {
            LOG_CRITICAL("%s manager shutdown duration:%ld exceeded threshold duration:%ld ms",
                Name(), duration.count(), TEARDOWN_THRESHOLD.count());
            break;
        }
        LOG_INFO("%s manager shutdown duration:%ld ms", Name(), duration.count());
    }

    LOG_INFO("%s manager shutdown complete", Name());
}

void ManagerThread::SendEventsToWorkers()
{
    std::lock_guard lock{ m_workersMtx };

    if (m_workersTerminated)
    {
        LOG_WARNING("%s workers terminated", Name());
        return;
    }

    for (auto worker : m_workers)
    {
        LOG_INFO("%s sending work to %s", Name(), worker->Name());
        worker->TransmitEvent(std::make_unique<ManagerWorkerTestEvent>(m_testTimeout));
        LOG_DEBUG("%s completed sending work to %s", Name(), worker->Name());
    }
}

void ManagerThread::TeardownWorkers()
{
    std::lock_guard lock{ m_workersMtx };

    if (m_workersTerminated)
    {
        LOG_CRITICAL("%s workers termination has already been requested", Name());
        return;
    }

    m_workersTerminated = true;

    LOG_INFO("%s stopping transmit timer", Name());
    RemoveTimer(ManagerTimerEvent::TransmitWork);

    LOG_INFO("%s tearing down all workers", Name());
    for (auto worker : m_workers)
    {
        LOG_INFO("%s stopping %s", Name(), worker->Name());
        worker->Stop();
    }

    LOG_INFO("%s stop requested for all workers", Name());
}

void  ManagerThread::InitiateShutdown()
{
    LOG_INFO("%s initiating shutdown", Name());

    TeardownWorkers();
    m_shutdownInitiatedSignal.release();

    LOG_INFO("%s initiated shutdown", Name());
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
    LOG_INFO("%s setting up periodic timer for self transmitting", Name());

    AddPeriodicTimer(ManagerTimerEvent::TransmitWork, m_transmitPeriod);
    StartTimer(ManagerTimerEvent::TransmitWork);
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
                    LOG_ERROR("%s handle-event got unkown timer event:%d", Name(), event.Type());
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
                case ManagerEvent::Shutdown:
                {
                    InitiateShutdown();
                    break;
                }

                default:
                {
                    LOG_ERROR("%s handle-event got unkown manager event:%d", Name(), event.Type());
                    break;
                }
            }

            break;
        }

        default:
        {
            LOG_ERROR("%s handle-event got event from unexpected receiver:%s",
                Name(), threadEvent->ReceiverName());
            break;
        }
    }
}

} // namespace Sage::Threading
