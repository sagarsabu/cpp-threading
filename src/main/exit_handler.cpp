#include <csignal>
#include <cstring>
#include <array>

#include "main/exit_handler.hpp"
#include "log/logger.hpp"


namespace Sage::ExitHandler
{

sigset_t g_signalsToBlock{};

void Setup()
{
    // This runs in the main thread so these signals are blocked for all threads
    // Ensures setup is only triggered once
    static auto initialized = []
    {
        sigemptyset(&g_signalsToBlock);

        const std::array sigsToBlock{ SIGINT, SIGQUIT, SIGHUP, SIGTERM };
        for (auto sig : sigsToBlock)
        {
            sigaddset(&g_signalsToBlock, sig);
        }

        if (pthread_sigmask(SIG_BLOCK, &g_signalsToBlock, nullptr) != 0)
        {
            Log::Error("failed to block exit signals. e: %s", strerror(errno));
        }
        else
        {
            Log::Info("successfully blocked exit signals");
        }

        return 0;
    }();
    (void) initialized;
}

void WaitForExit(const ExitHandlerCallBack&& theExitHandle)
{
    ExitHandlerCallBack exitHandle{ theExitHandle };
    int receivedSignal{ 0 };

    while (true)
    {
        if (sigwait(&g_signalsToBlock, &receivedSignal) != 0)
        {
            Log::Error("sigwait failed when waiting for exit. e: %s", strerror(errno));
            continue;
        }

        switch (receivedSignal)
        {
            case SIGINT:
            case SIGQUIT:
            case SIGHUP:
            case SIGTERM:
            {
                Log::Info("exit-handler received signal '%s'. triggering exit-handle.", strsignal(receivedSignal));
                exitHandle();
                return;
            }

            default:
            {
                Log::Error("got unexpected signal:%d", receivedSignal);
                break;
            }
        }
    }


}

} // namespace Sage::ExitHandler
