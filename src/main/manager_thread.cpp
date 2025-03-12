#include <algorithm>
#include <chrono>

#include "log/logger.hpp"
#include "main/manager_thread.hpp"

#include "threading/events.hpp"

namespace Sage::Threading
{

enum ManagerTimerEvent
{
    TransmitWork
};

ManagerThread::ManagerThread() : Thread{ "MngrThread" } {}

void ManagerThread::AttachWorker(Thread* worker)
{
    std::lock_guard lock{ m_workersMtx };
    m_workers.emplace(worker);
}

void ManagerThread::RequestShutdown()
{
    LOG_INFO("shutdown requested for '{}'", Name());
    m_shutdownInitiateSignal.release();
}

// Called from main thread
void ManagerThread::WaitForShutdown()
{
    LOG_INFO("waiting for shutdown initiate signal for '{}'", Name());
    m_shutdownInitiateSignal.acquire();
    LOG_INFO("shutdown initiate signal for '{}' acquired", Name());

    TransmitEvent(std::make_unique<ManagerShutdownEvent>());

    LOG_INFO("waiting for shutdown initiated signal for '{}'", Name());
    m_shutdownInitiatedSignal.acquire();
    LOG_INFO("shutdown initiated signal for '{}' acquired", Name());

    // Initiate a stop request for the manager thread
    Stop();

    TryWaitForWorkersShutdown();
    TryWaitForManagerShutdown();
}

void ManagerThread::TryWaitForWorkersShutdown()
{
    LOG_INFO("{} workers shutdown started", Name());

    auto workerTeardownStart = Clock::now();
    while (WorkersRunning())
    {
        std::this_thread::sleep_for(20ms);
        auto now = Clock::now();
        auto duration = std::chrono::duration_cast<TimeMS>(now - workerTeardownStart);

        if (duration >= TEARDOWN_THRESHOLD)
        {
            LOG_CRITICAL(
                "{} workers shutdown duration:{} exceeded threshold duration:{}",
                Name(),
                duration.count(),
                TEARDOWN_THRESHOLD
            );
            break;
        }
        LOG_INFO("{} workers shutdown duration:{}", Name(), duration);
    }

    LOG_INFO("{} workers shutdown complete", Name());
}

void ManagerThread::TryWaitForManagerShutdown()
{
    LOG_INFO("{} manager shutdown starting", Name());

    auto managerTeardownStart = Clock::now();
    while (IsRunning())
    {
        std::this_thread::sleep_for(20ms);
        auto now = Clock::now();
        auto duration = std::chrono::duration_cast<TimeMS>(now - managerTeardownStart);

        if (duration >= TEARDOWN_THRESHOLD)
        {
            LOG_CRITICAL(
                "{} manager shutdown duration:{} exceeded threshold duration:{}",
                Name(),
                duration.count(),
                TEARDOWN_THRESHOLD
            );
            break;
        }
        LOG_INFO("{} manager shutdown duration:{}", Name(), duration);
    }

    LOG_INFO("{} manager shutdown complete", Name());
}

void ManagerThread::SendEventsToWorkers()
{
    std::lock_guard lock{ m_workersMtx };

    if (m_workersTerminated) [[unlikely]]
    {
        LOG_WARNING("{} workers terminated", Name());
        return;
    }

    for (auto worker : m_workers)
    {
        LOG_INFO("{} sending work to {}", Name(), worker->Name());
        worker->TransmitEvent(std::make_unique<ManagerWorkerTestEvent>(m_testTimeout));
        LOG_DEBUG("{} completed sending work to {}", Name(), worker->Name());
    }
}

void ManagerThread::TeardownWorkers()
{
    std::lock_guard lock{ m_workersMtx };

    if (m_workersTerminated)
    {
        LOG_CRITICAL("{} workers termination has already been requested", Name());
        return;
    }

    m_workersTerminated = true;

    LOG_INFO("{} stopping transmit timer", Name());
    RemoveTimer(ManagerTimerEvent::TransmitWork);

    LOG_INFO("{} tearing down all workers", Name());
    for (auto worker : m_workers)
    {
        LOG_INFO("{} stopping {}", Name(), worker->Name());
        worker->Stop();
    }

    LOG_INFO("{} stop requested for all workers", Name());
}

void ManagerThread::InitiateShutdown()
{
    LOG_INFO("{} initiating shutdown", Name());

    TeardownWorkers();
    m_shutdownInitiatedSignal.release();

    LOG_INFO("{} initiated shutdown", Name());
}

bool ManagerThread::WorkersRunning()
{
    std::lock_guard lock{ m_workersMtx };

    bool aWorkerIsRunning =
        std::any_of(m_workers.cbegin(), m_workers.cend(), [](const Thread* worker) { return worker->IsRunning(); });
    return aWorkerIsRunning;
}

void ManagerThread::Starting()
{
    LOG_INFO("{} setting up periodic timer for self transmitting", Name());

    AddPeriodicTimer(ManagerTimerEvent::TransmitWork, m_transmitPeriod);
    StartTimer(ManagerTimerEvent::TransmitWork);
}

void ManagerThread::HandleEvent(UniqueThreadEvent threadEvent)
{
    switch (threadEvent->Receiver())
    {
        case EventReceiver::Timer:
        {
            const auto& event = static_cast<const TimerEvent&>(*threadEvent);
            switch (event.Type())
            {
                case ManagerTimerEvent::TransmitWork:
                {
                    SendEventsToWorkers();
                    break;
                }

                default:
                {
                    LOG_ERROR("{} handle-event got unkown timer event:{}", Name(), event.Type());
                    break;
                }
            }

            break;
        }

        case EventReceiver::ManagerThread:
        {

            const auto& event = static_cast<const ManagerEvent&>(*threadEvent);
            switch (event.Type())
            {
                case ManagerEvent::Shutdown:
                {
                    InitiateShutdown();
                    break;
                }

                default:
                {
                    LOG_ERROR("{} handle-event got unkown manager event:{}", Name(), (int)event.Type());
                    break;
                }
            }

            break;
        }

        default:
        {
            LOG_ERROR("{} handle-event got event from unexpected receiver:{}", Name(), threadEvent->ReceiverName());
            break;
        }
    }
}

} // namespace Sage::Threading
