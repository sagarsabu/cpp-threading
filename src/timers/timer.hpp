#pragma once

#include <chrono>
#include <ctime>
#include <functional>

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

constexpr timespec ChronoTimeToTimeSpec(const TimeNS& duration) noexcept
{
    const TimeS seconds{ std::chrono::duration_cast<TimeS>(duration) };
    const TimeNS nanoSecs{ duration };
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

    const std::string& Name() const noexcept { return m_name; };

protected:
    Timer(const std::string& name, const TimeNS& startDelta, const TimeNS& period, TimerCallback&& callback);

private:
    // Nothing in here is movable or copyable
    Timer(const Timer&) = delete;
    Timer(Timer&&) = delete;
    Timer& operator=(const Timer&) = delete;
    Timer& operator=(Timer&&) = delete;

    struct SigValData
    {
        const Timer& m_theTimer;
    };

private:
    timer_t m_timer{};
    const itimerspec m_timerInterval;
    const TimerCallback m_callback;
    const std::string m_name{ "Unknown" };
    const SigValData m_signalData;

private:
    static constexpr itimerspec DISABLED_TIMER{ .it_interval = ChronoTimeToTimeSpec(0ns),
                                                .it_value = ChronoTimeToTimeSpec(0ns) };
};

// Fire once timer. Has to be restarted

class FireOnceTimer final : public Timer
{
public:
    FireOnceTimer(const std::string& name, const TimeNS& delta, TimerCallback&& callback);

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
    PeriodicTimer(const std::string& name, const TimeNS& period, TimerCallback&& callback);

private:
    // Nothing in here is movable or copyable
    PeriodicTimer(const PeriodicTimer&) = delete;
    PeriodicTimer(PeriodicTimer&&) = delete;
    PeriodicTimer& operator=(const PeriodicTimer&) = delete;
    PeriodicTimer& operator=(PeriodicTimer&&) = delete;
};

} // namespace Sage
