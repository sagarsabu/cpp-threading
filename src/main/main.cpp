#include <string>
#include <chrono>
#include <atomic>
#include <csignal>
#include <memory>

#include "log/logger.hpp"
#include "thread/thread.hpp"
#include "thread/events.hpp"
#include "main/thread_manager.hpp"

using namespace std::chrono_literals;
using namespace Sage;
using namespace Sage::Thread;

namespace Sage::Thread
{

std::atomic<ManagerThread*> g_managerThread{ nullptr };

class WorkerThread final : public ThreadI
{
public:
    WorkerThread() :
        ThreadI{ std::string("WkrThread-") + std::to_string(++s_id) }
    { }

private:
    void HandleEvent(std::unique_ptr<Event> event) override
    {
        switch (event->Type())
        {
            case ManagerEventT::Test:
            {
                auto& rxEvent = static_cast<ManagerTestEvent&>(*event);
                Log::Info("%s handle-event 'Test'. sleeping for %ld ms",
                    Name(), rxEvent.m_timeout.count());
                std::this_thread::sleep_for(rxEvent.m_timeout);
                break;
            }

            default:
                Log::Error("%s handle-event unknown event:%d", Name(), event->Type());
                break;
        }
    }

private:
    static inline std::atomic<uint> s_id{ 0 };
};

} // namespace Sage::Thread

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
    std::signal(SIGINT, SignalHandler);
    std::signal(SIGQUIT, SignalHandler);
    std::signal(SIGKILL, SignalHandler);

    Log::SetLogLevel(Log::Level::Debug);

    ManagerThread manager;
    g_managerThread = &manager;
    manager.Start();

    std::array<WorkerThread, 10> workers;
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

    return manager.ExitCode();
}
