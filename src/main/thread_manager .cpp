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
    m_workersTerminated{ false }
{ }

void ManagerThread::AttachWorker(ThreadI* worker)
{
    m_workers.insert(worker);
}

void ManagerThread::TeardownWorkers()
{
    m_workersTerminated = true;

    for (auto worker : m_workers)
    {
        Log::info("%s Shutting down %s", Name(), worker->Name());
        worker->Shutdown();
    }

    while (WorkersStillRunning())
    {
        std::this_thread::sleep_for(100ms);
    }
}

void  ManagerThread::SignalHandler(int)
{
    std::unique_lock lock{ EXIT_REQ_MTX };
    EXIT_REQUESTED = true;
    EXIT_REQ_CND_VAR.notify_one();
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
            Log::info("%s received exit event", Name());
            break;
        }
    }

    for (auto worker : m_workers)
        nFailedWorkers += (worker->ExitCode() != 0);

    return nFailedWorkers;
}

void ManagerThread::SendEventsToWorkers()
{
    if (m_workersTerminated)
        return;

    for (auto worker : m_workers)
    {
        Log::info("%s Sending work to %s", Name(), worker->Name());
        worker->TransmitEvent(std::make_unique<Event>(EventT::Test));
        Log::info("%s Completed sending work to %s", Name(), worker->Name());
    }
}

void ManagerThread::Stopping()
{
    std::unique_lock lock{ SHUTDOWN_COMPLETE_MTX };
    SHUTDOWN_COMPLETED = true;
    SHUTDOWN_CND_VAR.notify_one();
}

bool ManagerThread::WorkersStillRunning() const
{
    bool aWorkerIsRunning = std::any_of(
        m_workers.begin(),
        m_workers.end(),
        [](ThreadI* worker) { return worker->IsRunning(); }
    );
    return aWorkerIsRunning;
}


} // namespace Sage::Thread
