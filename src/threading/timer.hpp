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

    void Start() const;

    void Stop() const;

protected:
    Timer(const TimeMilliSec& startDeltaMS, const TimeMilliSec& periodMS, const TimerCallback& callback);

private:
    // Nothing in here is movable or copyable
    Timer(const Timer&) = delete;
    Timer(Timer&&) = delete;
    Timer& operator=(const Timer&) = delete;
    Timer& operator=(Timer&&) = delete;

private:
    timer_t m_timerId;
    const itimerspec m_timerInterval;
    const TimerCallback m_callback;
    const uint m_id;

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
    FireOnceTimer();

    FireOnceTimer(const TimeMilliSec& deltaMS, const TimerCallback& callback);

private:
    // Nothing in here is movable or copyable
    FireOnceTimer(const FireOnceTimer&) = delete;
    FireOnceTimer(FireOnceTimer&&) = delete;
    FireOnceTimer& operator=(const FireOnceTimer&) = delete;
    FireOnceTimer& operator=(FireOnceTimer&&) = delete;

};

// Periodic timer

class PeriodicTimer final : public Timer
{
public:
    PeriodicTimer();

    PeriodicTimer(const TimeMilliSec& periodMS, const TimerCallback& callback);

private:
    // Nothing in here is movable or copyable
    PeriodicTimer(const PeriodicTimer&) = delete;
    PeriodicTimer(PeriodicTimer&&) = delete;
    PeriodicTimer& operator=(const PeriodicTimer&) = delete;
    PeriodicTimer& operator=(PeriodicTimer&&) = delete;

};

} // namespace Sage::Threading
