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

// Shutdown timer incase the process / a thread hangs
std::atomic<PeriodicTimer*> g_shutdownTimer{ nullptr };

void SignalHandler(int signal)
{
    static bool  shutdownTimerStarted{ false };

    Log::Info("signal-handler received signal:%d", signal);

    if (not shutdownTimerStarted)
    {
        shutdownTimerStarted = true;

        if (g_managerThread != nullptr)
        {
            Log::Info("signal-handler requesting exit");
            (*g_managerThread).RequestExit();
        }

        if (g_shutdownTimer != nullptr)
        {
            Log::Info("signal-handler starting timer");
            (*g_shutdownTimer).Start();
        }
    }
}

} // namespace Sage::Threading

int main(int, const char**)
{
    int res{ 0 };

    // Setup logging
    Logger::SetupLogger();
    Logger::SetLogLevel(Logger::Level::Debug);

    // Setup the the shutdown timer
    g_shutdownTimer = new PeriodicTimer(1000ms, []
    {
        constexpr auto shutdownThreshold{ 5000ms };
        static const auto shutdownStart{ Clock::now() };

        auto now = Clock::now();
        auto duration = std::chrono::duration_cast<TimeMilliSec>(now - shutdownStart);
        if (duration >= shutdownThreshold)
        {
            // Can't be caught so the oS kill kill us
            Log::Critical("shutdown duration exceeded. forcing shutdown");
            raise(SIGKILL);
        }
        else
        {
            Log::Warning("shutdown duration at %ld ms", duration.count());
        }
    });

    // Attach signals
    std::signal(SIGINT, SignalHandler);
    std::signal(SIGQUIT, SignalHandler);
    std::signal(SIGTERM, SignalHandler);

    try
    {
        g_managerThread = new ManagerThread;
        auto& manager = (*g_managerThread);

        manager.Start();

        std::array<WorkerThread, 1> workers;
        for (auto& worker : workers)
        {
            worker.Start();
            manager.AttachWorker(&worker);
        }

        // Make sure main thread waits until exit is requested
        manager.WaitForExit();
        // Make sure main thread waits until shutdown is complete
        manager.WaitForShutdown();

        res = manager.ExitCode();

        delete g_managerThread;
        g_managerThread = nullptr;
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

    delete g_shutdownTimer;
    g_shutdownTimer = nullptr;
    return res;
}
