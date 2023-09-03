#include <atomic>
#include <csignal>

#include "log/logger.hpp"
#include "threading/manager_thread.hpp"
#include "threading/worker_thread.hpp"

using namespace Sage;
using namespace Sage::Threading;

namespace Sage::Threading
{

std::atomic<ManagerThread*> g_managerThread{ nullptr };

void SignalHandler(int signal)
{
    Log<Info>("signal-handler received signal:%d", signal);

    // Shutdown timer incase the process hangs
    static bool  shutdownTimerStarted{ false };
    static const auto shutdownStart = Clock::now();
    static const auto shutdownThreshold{ 5000ms };
    static const auto shutdownTick{ 1000ms };

    // The shutdown timer
    static PeriodicTimer shutdownTimer(shutdownTick, [&]
    {
        auto now = Clock::now();
        auto duration = std::chrono::duration_cast<TimeMilliSec>(now - shutdownStart);
        if (duration >= shutdownThreshold)
        {
            // Can't be caught so the oS kill kill us
            Log<Critical>("shutdown duration exceeded. forcing shutdown");
            raise(SIGKILL);
        }
        else
        {
            Log<Warning>("shutdown duration at %ld ms", duration.count());
        }
    });

    if (g_managerThread != nullptr)
    {
        (*g_managerThread).RequestExit();
    }

    if (not shutdownTimerStarted)
    {
        Log<Info>("signal-handler starting timer");
        shutdownTimerStarted = true;
        shutdownTimer.Start();
    }
}

} // namespace Sage::Threading

int main(int, const char**)
{
    // Setup logging
    Logger::SetupLogger();
    Logger::SetLogLevel(LogLevel::Debug);

    int res{ 0 };

    // Attach signals
    std::signal(SIGINT, SignalHandler);
    std::signal(SIGQUIT, SignalHandler);
    std::signal(SIGTERM, SignalHandler);

    try
    {
        g_managerThread = new ManagerThread;
        auto& manager = (*g_managerThread);

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

        delete g_managerThread;
        g_managerThread = nullptr;
    }
    catch (const std::exception& e)
    {
        Log<Critical>("Caught unexpected std exception. What: %s. Shutting down.", e.what());
        res = 2;
    }
    catch (...)
    {
        Log<Critical>("Caught unknown exception. Shutting down.");
        res = 1;
    }

    return res;
}
