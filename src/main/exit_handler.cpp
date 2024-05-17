#include <csignal>
#include <cstring>
#include <array>

#include "main/exit_handler.hpp"
#include "log/logger.hpp"
#include "timers/timer.hpp"

namespace Sage::ExitHandler
{

void AttachExitHandler(ExitHandle&& theExitHandle)
{
    static bool attached{ false };
    if (attached)
    {
        Log::Critical("exit handler has already been attached");
        std::raise(SIGKILL);
    }


    sigset_t signalsToBlock{};
    sigemptyset(&signalsToBlock);

    const std::array exitSignals{ SIGINT, SIGQUIT, SIGHUP, SIGTERM };
    for (auto sig : exitSignals)
    {
        sigaddset(&signalsToBlock, sig);
    }

    if (pthread_sigmask(SIG_BLOCK, &signalsToBlock, nullptr) != 0)
    {
        Log::Critical("failed to block exit signals. e: %s", strerror(errno));
    }
    else
    {
        Log::Info("successfully blocked exit signals");
    }

    std::thread([exitHandle = std::move(theExitHandle), signalsToBlock = std::move(signalsToBlock)]
    {
        pthread_setname_np(pthread_self(), "ExitHandler");

        int receivedSignal{ 0 };
        bool triggered{ false };

        constexpr auto shutdownThreshold{ 5s };
        constexpr auto shutdownTick{ 500ms };
        PeriodicTimer shutdownTimer(shutdownTick, [&]
        {
            static const auto shutdownStart{ Clock::now() - shutdownTick };

            auto now = Clock::now();
            auto duration = std::chrono::duration_cast<TimeMS>(now - shutdownStart);
            if (duration >= shutdownThreshold)
            {
                // Can't be caught so the os will kill us
                Log::Critical("shutdown duration exceeded. forcing shutdown");
                std::raise(SIGKILL);
            }
            else
            {
                Log::Warning("shutdown duration at %ld ms", duration.count());
            }
        });

        Log::Info("exit-handler waiting for exit signal");

        while (true)
        {
            if (sigwait(&signalsToBlock, &receivedSignal) != 0)
            {
                Log::Critical("sigwait failed when waiting for exit. e: %s", strerror(errno));
                continue;
            }

            switch (receivedSignal)
            {
                case SIGINT:
                case SIGQUIT:
                case SIGHUP:
                case SIGTERM:
                {
                    if (not triggered)
                    {
                        triggered = true;
                        Log::Info("exit-handler received signal '%s'. triggering exit-handle.", strsignal(receivedSignal));
                        shutdownTimer.Start();
                    }
                    else
                    {
                        Log::Critical("exit-handler received additional signal '%s'. triggering exit-handle again.", strsignal(receivedSignal));
                    }

                    exitHandle();
                    break;
                }

                default:
                {
                    Log::Critical("got unexpected signal:%d", receivedSignal);
                    break;
                }
            }
        }
    }).detach();
}

} // namespace Sage::ExitHandler
