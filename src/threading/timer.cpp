#include <chrono>
#include <cstring>
#include <csignal>

#include "threading/timer.hpp"

namespace Sage::Threading
{


inline uint GetNextTimerID()
{
    static uint rollingTimerId{ 0 };

    if (++rollingTimerId == 0)
        rollingTimerId = 1;

    return rollingTimerId;
}

// Base timer

Timer::Timer(const TimeMilliSec& startDeltaMS, const TimeMilliSec& periodMS, const TimerCallback& callback) :
    m_timerId{},
    m_timerInterval{
        .it_interval = MilliSecsToTimeSpec(periodMS),
        .it_value = MilliSecsToTimeSpec(startDeltaMS)
    },
    m_callback{ callback },
    m_id{ GetNextTimerID() }
{
    Log<Debug>("creating timer with id:%d", m_id);

    sigevent signalEvent{};
    struct sigaction signalAction {};

    signalEvent.sigev_signo = SIGRTMIN;
    // So we send the signal to the same the thread that created the timer
    signalEvent.sigev_notify = SIGEV_THREAD_ID;
    signalEvent._sigev_un._tid = gettid();
    // The callback is passed in as the signal value to be called from the signal handler
    signalEvent.sigev_value.sival_ptr = const_cast<TimerCallback*>(&m_callback);
    // setup signal handler
    signalAction.sa_flags = SA_SIGINFO;
    // Set the callback for the signal
    signalAction.sa_sigaction = [](int /**sig*/, siginfo_t* si, void* /**uc*/) -> void
    {
        auto callback = static_cast<const TimerCallback*>(si->si_ptr);
        Log<Debug>("triggering callback for timer-id:%d", si->si_timerid);
        (*callback)();
    };

    // clear out any signals
    sigemptyset(&signalAction.sa_mask);

    // Setup the signal handler
    if (sigaction(signalEvent.sigev_signo, &signalAction, nullptr) == -1)
    {
        Log<Critical>("failed to set signal action for id:%d. %s", m_id, strerror(errno));
        return;
    }

    // setup the timer
    if (timer_create(CLOCK_MONOTONIC, &signalEvent, &m_timerId) != 0)
    {
        Log<Critical>("failed to create timer for id:%d. %s", m_id, strerror(errno));
        return;
    }

    Log<Debug>("successfully setup id:%d for thread-id:%d", m_id, gettid());
}

Timer::~Timer()
{
    if (timer_delete(m_timerId) != 0)
    {
        Log<Critical>("failed to delete timer for id:%d. %s", m_id, strerror(errno));
    }
}

void Timer::Start() const
{
    if (timer_settime(m_timerId, 0, &m_timerInterval, nullptr) != 0)
    {
        Log<Critical>("failed to start time for id:%d. %s", m_id, strerror(errno));
    }
}

void Timer::Stop() const
{
    if (timer_settime(m_timerId, 0, &DISABLED_TIMER, nullptr) != 0)
    {
        Log<Critical>("failed to stop time for id:%d. %s", m_id, strerror(errno));
    }
}

// Fire once timer

FireOnceTimer::FireOnceTimer() :
    Timer{ 0ms, 0ms, [] { } }
{ }

FireOnceTimer::FireOnceTimer(const TimeMilliSec& deltaMS, const TimerCallback& callback) :
    Timer{ deltaMS, 0ms, callback }
{ }

// Periodic timer

PeriodicTimer::PeriodicTimer() :
    Timer{ 0ms, 0ms, [] { } }
{ }

PeriodicTimer::PeriodicTimer(const TimeMilliSec& periodMS, const TimerCallback& callback) :
    Timer{ periodMS, periodMS, callback }
{ }

} // namespace Sage::Threading

