#include <string>
#include <chrono>
#include <atomic>
#include <csignal>
#include <memory>
#include <set>
#include <algorithm>

#include "log/logger.hpp"
#include "thread/thread.hpp"
#include "thread/events.hpp"
#include "main/thread_manager.hpp"


using namespace std::chrono_literals;


namespace Sage::Thread
{

class WorkerThread final : public ThreadI
{
public:
    explicit WorkerThread(int id) :
        ThreadI{ std::string("WkrThread-") + std::to_string(id) }
    { }

private:
    void HandleEvent(std::unique_ptr<Event> event) override
    {
        switch (event->Type())
        {
        case EventT::Test:
            Log::info("%s HandleEvent Test. Sleeping for 500 ms", Name());
            std::this_thread::sleep_for(500ms);
            break;

        default:
            Log::info("%s HandleEvent unknown event", Name());
            break;
        }

    }

};

} // namespace Sage::Thread


auto main(void) -> int
{
    using namespace Sage::Thread;

    std::signal(SIGINT, ManagerThread::SignalHandler);
    std::signal(SIGTERM, ManagerThread::SignalHandler);

    ManagerThread manager;
    std::vector<WorkerThread*> workers(1, nullptr);

    int idx = 1;
    for (auto worker : workers)
    {
        worker = new WorkerThread{ idx++ };
        manager.AttachWorker(worker);
    }

    {
        std::unique_lock lock{ ManagerThread::EXIT_REQ_MTX };
        ManagerThread::EXIT_REQ_CND_VAR.wait(lock, [] { return ManagerThread::EXIT_REQUESTED; });
    }

    manager.TeardownWorkers();
    for (auto worker : workers)
    {
        delete worker;
        worker = nullptr;
    }

    manager.Shutdown();

    {
        std::unique_lock lock{ ManagerThread::SHUTDOWN_COMPLETE_MTX };
        ManagerThread::SHUTDOWN_CND_VAR.wait(lock, [] { return ManagerThread::SHUTDOWN_COMPLETED; });
    }

    return manager.ExitCode();
}
