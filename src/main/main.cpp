#include <getopt.h>

#include <csignal>
#include <iostream>
#include <utility>

#include "log/logger.hpp"
#include "main/exit_handler.hpp"
#include "main/manager_thread.hpp"
#include "main/worker_thread.hpp"
#include "timers/timer_thread.hpp"

using namespace Sage;

auto GetCliArgs(int argc, char** const argv)
{
    static const option argOptions[]{
        { "help",  no_argument,       nullptr, 'h' },
        { "level", required_argument, nullptr, 'l' },
        { "file",  required_argument, nullptr, 'f' },
        { 0,       0,                 0,       0   }
    };

    auto usage = [&argv]
    {
        std::string_view progName{ argv[0] };
        if (size_t pos{ progName.find_last_of('/') }; pos != std::string::npos)
        {
            progName = progName.substr(pos + 1);
        }

        std::cout << "Usage: " << progName
                  << "\n\t[optional] --level|-l "
                     "<t|trace|d|debug|i|info|w|warn|e|error|c|critical>"
                     "\n\t[optional] --file|-f <filename> "
                     "\n\t[optional] --help|-h"
                  << std::endl;
    };

    auto getLogLevel = [](std::string_view logArg)
    {
        Logger::Level level{ Logger::Level::Info };
        if (logArg == "trace" or logArg == "t")
        {
            level = Logger::Level::Trace;
        }
        else if (logArg == "debug" or logArg == "d")
        {
            level = Logger::Level::Debug;
        }
        else if (logArg == "info" or logArg == "i")
        {
            level = Logger::Level::Info;
        }
        else if (logArg == "warn" or logArg == "w")
        {
            level = Logger::Level::Warning;
        }
        else if (logArg == "error" or logArg == "e")
        {
            level = Logger::Level::Error;
        }
        else if (logArg == "critical" or logArg == "c")
        {
            level = Logger::Level::Critical;
        }

        return level;
    };

    Logger::Level logLevel{ Logger::Info };
    std::string logFile;

    int option;
    int optIndex;
    while ((option = getopt_long(argc, argv, "hl:f:", argOptions, &optIndex)) != -1)
    {
        switch (option)
        {
            case 'h':
                usage();
                std::exit(0);
                break;

            case 'l':
                logLevel = getLogLevel(optarg);
                break;

            case 'f':
                logFile = optarg;
                break;

            case '?':
            default:
                usage();
                std::exit(1);
                break;
        }
    }

    return std::make_pair(logLevel, logFile);
}

int main(int argc, char** const argv)
{
    int res{ 0 };

    try
    {
        auto [logLevel, logFile]{ GetCliArgs(argc, argv) };

        // Setup logging
        Logger::SetupLogger(logFile, logLevel);

        LOG_INFO("==== starting ====");

        ManagerThread* managerPtr{ nullptr };
        std::jthread exitHandler = ExitHandler::Create(
            [&managerPtr]
            {
                LOG_INFO("exit-handle triggered");
                if (managerPtr != nullptr)
                {
                    managerPtr->RequestShutdown();
                }
            }
        );

        TimerThread timerThread;
        timerThread.Start();

        ManagerThread manager{ timerThread };
        managerPtr = &manager;

        manager.SetTransmitPeriod(20ms);
        manager.Start();

        std::array workers{ WorkerThread{ timerThread }, WorkerThread{ timerThread } };
        for (auto& worker : workers)
        {
            worker.Start();
            manager.AttachWorker(&worker);
        }

        // Make sure main thread waits until shutdown is complete
        manager.WaitForShutdown();
        res = manager.ExitCode();
        exitHandler.request_stop();
    }
    catch (const std::exception& e)
    {
        LOG_CRITICAL("caught unexpected std exception. what:{}. shutting down.", e.what());
        res = 2;
    }
    catch (...)
    {
        LOG_CRITICAL("caught unknown exception. shutting down.");
        res = 1;
    }

    LOG_INFO("==== terminating with return-code:{} ====", res);
    return res;
}
