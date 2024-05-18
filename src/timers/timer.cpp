#include <csignal>
#include <cstring>

#include "log/logger.hpp"
#include "timers/timer.hpp"

namespace Sage
{

inline uint GetNextTimerID()
{
    static uint rollingTimerId{ 0 };

    if (++rollingTimerId == 0)
        rollingTimerId = 1;

    return rollingTimerId;
}

// Base timer

Timer::Timer(const TimeNS& startDelta, const TimeNS& period, TimerCallback&& callback) :
    m_timerInterval{ .it_interval = NanoSecsToTimeSpec(period), .it_value = NanoSecsToTimeSpec(startDelta) },
    m_signalData{ .m_callback = std::move(callback), .m_timerId = GetNextTimerID() }
{
    LOG_DEBUG("c'tor timer with id:%d", Id());

    sigevent signalEvent{};
    struct sigaction signalAction {};

    signalEvent.sigev_signo = SIGRTMIN;
    // So we send the signal to the same the thread that created the timer
    signalEvent.sigev_notify = SIGEV_THREAD_ID;
    signalEvent._sigev_un._tid = gettid();
    // The signal data to be used from the signal handler
    signalEvent.sigev_value.sival_ptr = const_cast<SigValData*>(&m_signalData);
    // setup signal handler
    signalAction.sa_flags = SA_SIGINFO;
    // Set the callback for the signal
    signalAction.sa_sigaction = [](int /**sig*/, siginfo_t* si, void* /**uc*/) -> void
    {
        auto signalData = static_cast<const SigValData*>(si->si_ptr);
        LOG_TRACE("triggering callback for timer-id:%d", signalData->m_timerId);
        (signalData->m_callback)();
    };

    // clear out any signals
    sigemptyset(&signalAction.sa_mask);

    // Setup the signal handler
    if (sigaction(signalEvent.sigev_signo, &signalAction, nullptr) == -1)
    {
        LOG_CRITICAL("failed to set signal action for id:%d. %s", Id(), strerror(errno));
        return;
    }

    // setup the timer
    if (timer_create(CLOCK_MONOTONIC, &signalEvent, &m_timer) != 0)
    {
        LOG_CRITICAL("failed to create timer for id:%d. %s", Id(), strerror(errno));
        return;
    }

    LOG_TRACE("successfully setup id:%d for thread-id:%d", Id(), gettid());
}

Timer::~Timer()
{
    LOG_DEBUG("timer id:%d d'tor", Id());

    if (timer_delete(m_timer) != 0)
    {
        LOG_CRITICAL("failed to delete timer for id:%d. %s", Id(), strerror(errno));
    }
}

void Timer::Start() const
{
    if (timer_settime(m_timer, 0, &m_timerInterval, nullptr) != 0)
    {
        LOG_CRITICAL("failed to start time for id:%d. %s", Id(), strerror(errno));
    }
}

void Timer::Stop() const
{
    if (timer_settime(m_timer, 0, &DISABLED_TIMER, nullptr) != 0)
    {
        LOG_CRITICAL("failed to stop time for id:%d. %s", Id(), strerror(errno));
    }
}

// Fire once timer

FireOnceTimer::FireOnceTimer() :
    Timer{ 0ns, 0ns, [] { } }
{ }

FireOnceTimer::FireOnceTimer(const TimeNS& delta, TimerCallback&& callback) :
    Timer{ delta, 0ns, std::move(callback) }
{ }

// Periodic timer

PeriodicTimer::PeriodicTimer() :
    Timer{ 0ns, 0ns, [] { } }
{ }

PeriodicTimer::PeriodicTimer(const TimeNS& period, TimerCallback&& callback) :
    Timer{ period, period, std::move(callback) }
{ }

} // namespace Sage

