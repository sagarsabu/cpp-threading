#include <atomic>
#include <csignal>

#include "log/logger.hpp"
#include "threading/manager_thread.hpp"
#include "threading/worker_thread.hpp"

using namespace Sage;
using namespace Sage::Threading;

std::atomic<Sage::Threading::ManagerThread*> g_managerThread{ nullptr };

void SignalHandler(int signal)
{
    Log::Info("SignalHandler received signal:%d", signal);
    if (g_managerThread != nullptr)
    {
        (*g_managerThread).RequestExit();
    }
}

auto main(void) -> int
{
    int res{ 0 };

    // Attach signals
    std::signal(SIGINT, SignalHandler);
    std::signal(SIGQUIT, SignalHandler);

    // Setup logging
    Log::SetupLogger();
    Log::SetLogLevel(Log::Level::Debug);

    try
    {
        ManagerThread manager;
        g_managerThread = &manager;
        manager.Start();

        std::array<WorkerThread, 4> workers;
        for (auto& worker : workers)
        {
            worker.Start();
            manager.AttachWorker(&worker);
        }

        // Make sure main thread waits until exit is requested
        manager.WaitForExit();
        // Make sure main thread waits until shutdown is complete
        manager.WaitUntilWorkersShutdown();
        manager.WaitUntilManagerShutdown();

        res = manager.ExitCode();
    }
    catch (const std::exception& e)
    {
        Log::Critical("Caught unexpected std exception. What: %s. Shutting down.", e.what());
        res = 2;
    }
    catch (...)
    {
        Log::Critical("Caught unknown exception. Shutting down.");
        res = 1;
    }

    Log::TeardownLogger();
    return res;
}
