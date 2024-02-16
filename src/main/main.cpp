#include <atomic>
#include <csignal>

#include "log/logger.hpp"
#include "threading/manager_thread.hpp"
#include "threading/worker_thread.hpp"
#include "timers/timer.hpp"
#include "main/exit_handler.hpp"

using namespace Sage;

int main(int argc, const char** argv)
{
    int res{ 0 };

    try
    {
        Logger::Level level{ Logger::Level::Info };
        if (argc >= 2 and (std::string_view{ argv[1] } == "--debug" or std::string_view{ argv[1] } == "-d"))
        {
            level = Logger::Level::Debug;
        }

        // Setup logging
        Logger::SetupLogger();
        Logger::SetLogLevel(level);

        ExitHandler::Setup();

        Threading::ManagerThread manager;
        manager.Start();

        std::array<Threading::WorkerThread, 2> workers;
        for (auto& worker : workers)
        {
            worker.Start();
            manager.AttachWorker(&worker);
        }

        ExitHandler::WaitForExit([&]
        {
            Log::Info("exit-handle triggered");
            manager.RequestExit();

            PeriodicTimer shutdownTimer(1000ms, []
            {
                constexpr auto shutdownThreshold{ 5000ms };
                static const auto shutdownStart{ Clock::now() };

                auto now = Clock::now();
                auto duration = std::chrono::duration_cast<TimeMilliSec>(now - shutdownStart);
                if (duration >= shutdownThreshold)
                {
                    // Can't be caught so the os kill kill us
                    Log::Critical("shutdown duration exceeded. forcing shutdown");
                    raise(SIGKILL);
                }
                else
                {
                    Log::Warning("shutdown duration at %ld ms", duration.count());
                }
            });

            Log::Info("exit-handle starting shutdown timer");
            shutdownTimer.Start();

            // Make sure main thread waits until exit is requested
            manager.WaitForExit();
            // Make sure main thread waits until shutdown is complete
            manager.WaitForShutdown();

            res = manager.ExitCode();

        });
    }
    catch (const std::exception& e)
    {
        Log::Critical("caught unexpected std exception. what: %s. shutting down.", e.what());
        res = 2;
    }
    catch (...)
    {
        Log::Critical("caught unknown exception. Shutting down.");
        res = 1;
    }

    Log::Info("terminating with return-code:%d", res);
    return res;
}
