#pragma once

#include <atomic>
#include <functional>
#include <latch>
#include <memory>
#include <string>
#include <thread>
#include <unordered_map>

#include "channel/channel.hpp"
#include "threading/events.hpp"
#include "timers/time_utils.hpp"
#include "timers/timer_thread.hpp"

namespace Sage
{

// Aliases

using UniqueThreadEvent = std::unique_ptr<ThreadEvent>;

class Thread
{
public:
    using TimerExpiredCb = std::move_only_function<void()>;

    struct TimerData
    {
        std::string name;
        TimerExpiredCb cb;
    };

    Thread(
        const std::string& threadName, TimerThread& timerThread, const TimeMS& handleEventThreshold = 20ms,
        // must always be last
        Channel::ChannelPair<ThreadEvent> channel = Channel::MakeChannel<ThreadEvent>()
    );

    virtual ~Thread();

    const std::string& Name() const noexcept { return m_threadName; }

    void Start();

    void Stop();

    void TransmitEvent(UniqueThreadEvent event);

    int ExitCode() const noexcept { return m_exitCode; }

    bool IsRunning() const noexcept { return m_running; }

protected:
    virtual void Starting() {}

    virtual void Stopping() {}

    virtual void HandleEvent(UniqueThreadEvent event) = 0;

    TimerEventId StartTimer(const std::string& name, const TimeMS& timeout, TimerExpiredCb cb);

    void StopTimer(TimerEventId timerEventId);

private:
    // Not copyable or movable
    Thread(const Thread&) = delete;
    Thread(Thread&&) = delete;
    Thread& operator=(const Thread&) = delete;
    Thread& operator=(Thread&&) = delete;

    // thread entry point
    void Enter(std::unique_ptr<Channel::Rx<ThreadEvent>> rx);

    // main thread loop
    int Execute(std::unique_ptr<Channel::Rx<ThreadEvent>> rx);

    void ProcessEvents(Channel::Rx<ThreadEvent>& rx);

    // For loop back events for managing this thread
    void HandleSelfEvent(UniqueThreadEvent event);

private:
    const std::string m_threadName;
    std::shared_ptr<Channel::Tx<ThreadEvent>> m_tx;
    const TimeMS m_handleEventThreshold;
    std::unordered_map<TimerEventId, TimerData> m_timers{};
    std::atomic<int> m_exitCode{ 0 };
    std::latch m_startLatch{ 1 };
    std::atomic<bool> m_running{ false };
    std::atomic<bool> m_stopping{ false };
    TimerThread& m_timerThread;

    // must always be last
    std::jthread m_thread;

private:
    static constexpr size_t MAX_EVENTS_PER_LOOP{ 10 };
    static constexpr TimeMS PROCESS_EVENTS_THRESHOLD{ 1000ms };
    static constexpr TimeMS PROCESS_EVENTS_WAIT_TIMEOUT{ 100ms };
};

} // namespace Sage
