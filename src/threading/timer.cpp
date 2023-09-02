#include <chrono>
#include <cstring>
#include <csignal>

#include "threading/timer.hpp"

namespace Sage::Threading
{

// Base timer

Timer::Timer(const TimeMilliSec& startDeltaMS, const TimeMilliSec& periodMS, const TimerCallback& callback) :
    m_timerId{},
    m_timerInterval{
        .it_interval = MilliSecsToTimeSpec(periodMS),
        .it_value = MilliSecsToTimeSpec(startDeltaMS)
    }
{
    sigevent signalEvent{};
    struct sigaction signalAction {};

    signalEvent.sigev_signo = SIGRTMIN;
    // So we send the signal to the same the thread that created the timer
    signalEvent.sigev_notify = SIGEV_THREAD_ID;
    signalEvent._sigev_un._tid = gettid();
    // The callback is passed in as the signal value to be called from the signal callback
    // const cast should be fine as we're not modifying it
    signalEvent.sigev_value.sival_ptr = const_cast<TimerCallback*>(&callback);

    // setup signal handler
    signalAction.sa_flags = SA_SIGINFO;
    // Set the callback
    signalAction.sa_sigaction = [](int sig, siginfo_t* si, void* /**uc*/) -> void
    {
        auto callback = static_cast<const TimerCallback*>(si->si_ptr);
        Log::Debug("triggering callback for signal %d for timer-id %d", sig, si->si_timerid);
        if (callback != nullptr)
        {
            (*callback)();
        }
    };
    // clear out any signals
    sigemptyset(&signalAction.sa_mask);

    // Setup the signal handler
    if (sigaction(signalEvent.sigev_signo, &signalAction, nullptr) == -1)
    {
        Log::Critical("failed to set signal action. %s", strerror(errno));
        return;
    }

    // setup the timer
    if (timer_create(CLOCK_MONOTONIC, &signalEvent, &m_timerId) != 0)
    {
        Log::Critical("failed to create timer. %s", strerror(errno));
        return;
    }
}

Timer::~Timer()
{
    if (timer_delete(m_timerId) != 0)
    {
        Log::Critical("failed to delete timer. %s", strerror(errno));
    }
}

void Timer::Start()
{
    if (timer_settime(m_timerId, 0, &m_timerInterval, nullptr) != 0)
    {
        Log::Critical("failed to start time. %s", strerror(errno));
    }
}

void Timer::Stop()
{
    if (timer_settime(m_timerId, 0, &DISABLED_TIMER, nullptr) != 0)
    {
        Log::Critical("failed to start time. %s", strerror(errno));
    }
}

// Fire once timer

FireOnceTimer::FireOnceTimer(const TimeMilliSec& deltaMS, const TimerCallback& callback) :
    Timer{ deltaMS, 0ms, callback }
{ }

// Periodic timer

PeriodicTimer::PeriodicTimer(const TimeMilliSec& periodMS, const TimerCallback& callback) :
    Timer{ periodMS, periodMS, callback }
{ }

} // namespace Sage::Threading

