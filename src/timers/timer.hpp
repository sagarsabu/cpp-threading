#pragma once

#include <ctime>
#include <functional>
#include <chrono>


namespace Sage
{

// Makes life easier
using namespace std::chrono_literals;

// Useful aliases

using Clock = std::chrono::high_resolution_clock;
using TimeNS = std::chrono::nanoseconds;
using TimeUS = std::chrono::microseconds;
using TimeMS = std::chrono::milliseconds;
using TimeS = std::chrono::seconds;

// Helpers

constexpr timespec NanoSecsToTimeSpec(const TimeNS& duration)
{
    const TimeS& seconds = std::chrono::duration_cast<TimeS>(duration);
    const TimeNS& nanoSecs = duration;
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

    uint Id() const { return m_signalData.m_timerId; };

protected:
    Timer(const TimeNS& startDelta, const TimeNS& period, TimerCallback&& callback);

private:
    // Nothing in here is movable or copyable
    Timer(const Timer&) = delete;
    Timer(Timer&&) = delete;
    Timer& operator=(const Timer&) = delete;
    Timer& operator=(Timer&&) = delete;

    struct SigValData
    {
        const TimerCallback m_callback;
        const uint m_timerId;
    };

private:
    timer_t m_timer{};
    const itimerspec m_timerInterval;
    const SigValData m_signalData;

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

    FireOnceTimer(const TimeNS& delta, TimerCallback&& callback);

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

    PeriodicTimer(const TimeNS& period, TimerCallback&& callback);

private:
    // Nothing in here is movable or copyable
    PeriodicTimer(const PeriodicTimer&) = delete;
    PeriodicTimer(PeriodicTimer&&) = delete;
    PeriodicTimer& operator=(const PeriodicTimer&) = delete;
    PeriodicTimer& operator=(PeriodicTimer&&) = delete;
};

} // namespace Sage
