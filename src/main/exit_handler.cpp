#include <csignal>
#include <cstring>
#include <array>

#include "main/exit_handler.hpp"
#include "log/logger.hpp"
#include "timers/timer.hpp"

namespace Sage::ExitHandler
{

std::jthread Create(ExitHandle&& theExitHandle)
{
    static bool attached{ false };
    if (attached)
    {
        LOG_CRITICAL("exit handler has already been attached");
        std::exit(1);
    }
    attached = true;

    sigset_t signalsToBlock{};
    sigemptyset(&signalsToBlock);

    constexpr std::array exitSignals{ SIGINT, SIGQUIT, SIGHUP, SIGTERM };
    for (auto sig : exitSignals)
    {
        sigaddset(&signalsToBlock, sig);
    }

    if (pthread_sigmask(SIG_BLOCK, &signalsToBlock, nullptr) != 0)
    {
        LOG_CRITICAL("failed to block exit signals. e: {}", strerror(errno));
        std::exit(1);
    }

    LOG_INFO("successfully blocked exit signals");

    auto handler = [exitHandle = std::move(theExitHandle), signalsToBlock = std::move(signalsToBlock)](std::stop_token stopToken) -> void
    {
        pthread_setname_np(pthread_self(), "ExitHandler");

        constexpr auto shutdownThreshold{ 5s };
        constexpr auto shutdownTick{ 500ms };
        PeriodicTimer shutdownTimer("ExitHandlerShutdownTimer", shutdownTick, [&]
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
                LOG_WARNING("shutdown duration at {}", duration);
            }
        });

        LOG_INFO("exit-handler waiting for exit signal");

        bool triggered{ false };
        constexpr timespec sigWaitTimeout{ ChronoTimeToTimeSpec(100ms) };

        while (not stopToken.stop_requested())
        {
            int signalOrTimeout{ sigtimedwait(&signalsToBlock, nullptr, &sigWaitTimeout) };
            if (signalOrTimeout == -1)
            {
                int err{ errno };
                switch (err)
                {
                    // timeout
                    case EAGAIN:
                        break;

                    default:
                        LOG_CRITICAL("sigwait failed when waiting for exit. e: {}", strerror(err));
                        break;
                }

                continue;
            }

            switch (signalOrTimeout)
            {
                case SIGINT:
                case SIGQUIT:
                case SIGHUP:
                case SIGTERM:
                {
                    if (not triggered)
                    {
                        triggered = true;
                        LOG_INFO("exit-handler received signal '{}'. triggering exit-handle.", strsignal(signalOrTimeout));
                        shutdownTimer.Start();
                    }
                    else
                    {
                        LOG_CRITICAL("exit-handler received additional signal '{}'. triggering exit-handle again.", strsignal(signalOrTimeout));
                    }

                    exitHandle();
                    break;
                }

                default:
                {
                    LOG_CRITICAL("got unexpected signal '{}'", strsignal(signalOrTimeout));
                    break;
                }
            }
        }
    };

    return std::jthread{ std::move(handler) };
}

} // namespace Sage::ExitHandler
