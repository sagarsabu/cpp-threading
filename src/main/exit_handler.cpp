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
        LOG_CRITICAL("exit handler has already been attached");
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
        LOG_CRITICAL("failed to block exit signals. e: %s", strerror(errno));
    }
    else
    {
        LOG_INFO("successfully blocked exit signals");
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
                LOG_CRITICAL("shutdown duration exceeded. forcing shutdown");
                std::raise(SIGKILL);
            }
            else
            {
                LOG_WARNING("shutdown duration at %ld ms", duration.count());
            }
        });

        LOG_INFO("exit-handler waiting for exit signal");

        while (true)
        {
            if (sigwait(&signalsToBlock, &receivedSignal) != 0)
            {
                LOG_CRITICAL("sigwait failed when waiting for exit. e: %s", strerror(errno));
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
                        LOG_INFO("exit-handler received signal '%s'. triggering exit-handle.", strsignal(receivedSignal));
                        shutdownTimer.Start();
                    }
                    else
                    {
                        LOG_CRITICAL("exit-handler received additional signal '%s'. triggering exit-handle again.", strsignal(receivedSignal));
                    }

                    exitHandle();
                    break;
                }

                default:
                {
                    LOG_CRITICAL("got unexpected signal:%d", receivedSignal);
                    break;
                }
            }
        }
    }).detach();
}

} // namespace Sage::ExitHandler
