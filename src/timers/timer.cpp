#include <csignal>
#include <cstring>

#include "log/logger.hpp"
#include "timers/timer.hpp"

namespace Sage
{

// Base timer

Timer::Timer(const std::string& name, const TimeNS& startDelta, const TimeNS& period, TimerCallback&& callback) :
    m_timerInterval{ .it_interval = ChronoTimeToTimeSpec(period), .it_value = ChronoTimeToTimeSpec(startDelta) },
    m_callback{ std::move(callback) },
    m_name{ name },
    m_signalData{ .m_theTimer = *this }
{
    LOG_DEBUG("[{}] c'tor timer", Name());

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
        LOG_TRACE("[{}] triggering callback", signalData->m_theTimer.Name());
        (signalData->m_theTimer.m_callback)();
    };

    // clear out any signals
    sigemptyset(&signalAction.sa_mask);

    // Setup the signal handler
    if (sigaction(signalEvent.sigev_signo, &signalAction, nullptr) == -1)
    {
        LOG_CRITICAL("[{}] failed to set signal action. e: {}", Name(), strerror(errno));
        return;
    }

    // setup the timer
    if (timer_create(CLOCK_MONOTONIC, &signalEvent, &m_timer) != 0)
    {
        LOG_CRITICAL("[{}] failed to create timer. e: {}", Name(), strerror(errno));
        return;
    }

    LOG_TRACE("[{}] successfully setup for thread-id:{}", Name(), gettid());
}

Timer::~Timer()
{
    LOG_DEBUG("[{}] timer d'tor", Name());

    if (timer_delete(m_timer) != 0)
    {
        LOG_CRITICAL("[{}] failed to delete timer. e: {}", Name(), strerror(errno));
    }
}

void Timer::Start() const
{
    if (timer_settime(m_timer, 0, &m_timerInterval, nullptr) != 0)
    {
        LOG_CRITICAL("[{}] failed to start time. e: {}", Name(), strerror(errno));
    }
}

void Timer::Stop() const
{
    if (timer_settime(m_timer, 0, &DISABLED_TIMER, nullptr) != 0)
    {
        LOG_CRITICAL("[{}] failed to stop time. e: {}", Name(), strerror(errno));
    }
}

// Fire once timer

FireOnceTimer::FireOnceTimer(const std::string& name, const TimeNS& delta, TimerCallback&& callback) :
    Timer{ name, delta, 0ns, std::move(callback) }
{ }

// Periodic timer

PeriodicTimer::PeriodicTimer(const std::string& name, const TimeNS& period, TimerCallback&& callback) :
    Timer{ name, period, period, std::move(callback) }
{ }

} // namespace Sage

