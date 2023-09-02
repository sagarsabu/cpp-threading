#include <string>
#include <chrono>
#include <atomic>
#include <csignal>
#include <memory>

#include "log/logger.hpp"
#include "thread/thread.hpp"
#include "thread/events.hpp"
#include "thread/thread_manager.hpp"

using namespace std::chrono_literals;
using namespace Sage;
using namespace Sage::Threading;

namespace Sage::Threading
{

std::atomic<ManagerThread*> g_managerThread{ nullptr };

class WorkerThread final : public Thread
{
public:
    WorkerThread() :
        Thread{ std::string("WkrThread-") + std::to_string(++s_id) }
    { }

private:
    void HandleEvent(UniqueThreadEvent threadEvent) override
    {
        if (threadEvent->Receiver() != EventReceiverT::ThreadManager)
        {
            return;
        }

        auto& event = static_cast<ManagerEvent&>(*threadEvent);
        switch (event.Type())
        {
            case ManagerEventT::Test:
            {
                auto& rxEvent = static_cast<ManagerTestEvent&>(event);
                Log::Info("%s handle-event 'Test'. sleeping for %ld ms",
                    Name(), rxEvent.m_timeout.count());
                std::this_thread::sleep_for(rxEvent.m_timeout);
                break;
            }

            default:
                Log::Error("%s handle-event unknown event:%d",
                    Name(), static_cast<int>(event.Type()));
                break;
        }
    }

private:
    static inline std::atomic<uint> s_id{ 0 };
};

} // namespace Sage::Threading

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

        std::array<WorkerThread, 1> workers;
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
