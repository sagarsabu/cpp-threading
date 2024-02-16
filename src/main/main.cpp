#include <atomic>
#include <csignal>
#include <getopt.h>
#include <utility>
#include <iostream>

#include "log/logger.hpp"
#include "timers/timer.hpp"
#include "main/exit_handler.hpp"
#include "threading/manager_thread.hpp"
#include "threading/worker_thread.hpp"

using namespace Sage;

auto GetCLiArgs(int argc, char** const argv)
{
    static const option argOptions[]
    {
        {"help", no_argument, nullptr, 'h' },
        {"level", required_argument, nullptr, 'l' },
        {"file", required_argument, nullptr, 'f' },
        {0, 0, 0, 0}
    };

    auto usage = [&argv]
    {
        std::string_view progName{ argv[0] };
        if (size_t pos{ progName.find_last_of('/') }; pos != std::string::npos)
        {
            progName = progName.substr(pos + 1);
        }

        std::cout <<
            "Usage: " << progName <<
            "\n\t[optional] --level|-l <t|trace|d|debug|i|info|w|warn|e|error|c|critical>"
            "\n\t[optional] --file|-f <filename> "
            "\n\t[optional] --help|-h"
            << std::endl;
    };

    auto getLogLevel = [](const std::string_view& logArg)
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
    std::optional<std::string> logFile{};

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
        auto [logLevel, logFile] { GetCLiArgs(argc, argv) };

        // Setup logging
        Logger::SetupLogger(logFile);
        Logger::SetLogLevel(logLevel);

        Threading::ManagerThread* managerPtr{ nullptr };
        ExitHandler::CreateHandler([&managerPtr]
        {
            Log::Info("exit-handle triggered");
            if (managerPtr != nullptr)
            {
                managerPtr->RequestShutdown();
            }
        });

        Threading::ManagerThread manager;
        managerPtr = &manager;
        manager.Start();

        std::array<Threading::WorkerThread, 2> workers;
        for (auto& worker : workers)
        {
            worker.Start();
            manager.AttachWorker(&worker);
        }

        // Make sure main thread waits until shutdown is complete
        manager.WaitForShutdown();
        res = manager.ExitCode();
    }
    catch (const std::exception& e)
    {
        Log::Critical("caught unexpected std exception. what: %s. shutting down.", e.what());
        res = 2;
    }
    catch (...)
    {
        Log::Critical("caught unknown exception. shutting down.");
        res = 1;
    }

    Log::Info("terminating with return-code:%d", res);
    return res;
}
