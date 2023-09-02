#pragma once

#include <ctime>
#include <functional>
#include <chrono>

#include "log/logger.hpp"

namespace Sage::Threading
{
// Makes life easier
using namespace std::chrono_literals;

// Useful aliases

using Clock = std::chrono::high_resolution_clock;
using TimeMilliSec = std::chrono::milliseconds;
using TimeNanoSec = std::chrono::nanoseconds;
using TimeSec = std::chrono::seconds;

constexpr timespec MilliSecsToTimeSpec(const TimeMilliSec& duration)
{
    const TimeSec& seconds = std::chrono::duration_cast<TimeSec>(duration);
    const TimeNanoSec& nanoSecs = std::chrono::duration_cast<TimeNanoSec>(duration);
    return timespec{ seconds.count(), (nanoSecs - seconds).count() };
}

// Base timer

class Timer
{
public:
    using TimerCallback = std::function<void()>;

    virtual ~Timer();

    void Start();

    void Stop();

protected:
    Timer(const TimeMilliSec& startDeltaMS, const TimeMilliSec& periodMS, const TimerCallback& callback);

private:
    timer_t m_timerId;
    const itimerspec m_timerInterval;

private:
    static const inline itimerspec DISABLED_TIMER{
        .it_interval = {.tv_sec = 0, .tv_nsec = 0 },
        .it_value = {.tv_sec = 0, .tv_nsec = 0 }
    };
};

// Fire once timer. Has be restarted

class FireOnceTimer final : public Timer
{
public:
    FireOnceTimer(const TimeMilliSec& deltaMS, const TimerCallback& callback);

    ~FireOnceTimer() = default;

};

// Periodic timer

class PeriodicTimer final : public Timer
{
public:
    PeriodicTimer(const TimeMilliSec& periodMS, const TimerCallback& callback);

    ~PeriodicTimer() = default;

};

} // namespace Sage::Threading
