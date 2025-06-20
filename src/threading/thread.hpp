#pragma once

#include <atomic>
#include <latch>
#include <memory>
#include <string>
#include <thread>
#include <unordered_map>

#include "channel/channel.hpp"
#include "threading/events.hpp"
#include "timers/timer.hpp"

namespace Sage::Threading
{

// Aliases

using UniqueThreadEvent = std::unique_ptr<ThreadEvent>;

class Thread
{
public:
    Thread(
        const std::string& threadName, const TimeMS& handleEventThreshold = 20ms,
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

    void AddPeriodicTimer(TimerEvent::EventID timerEventId, TimeNS period);

    void AddFireOnceTimer(TimerEvent::EventID timerEventId, TimeNS delta);

    void RemoveTimer(TimerEvent::EventID timerEventId);

    void StartTimer(TimerEvent::EventID timerEventId) const;

    void StopTimer(TimerEvent::EventID timerEventId) const;

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
    std::unordered_map<TimerEvent::EventID, std::unique_ptr<Timer>> m_timers{};
    std::atomic<int> m_exitCode{ 0 };
    std::latch m_startLatch{ 1 };
    std::atomic<bool> m_running{ false };
    std::atomic<bool> m_stopping{ false };
    std::unique_ptr<FireOnceTimer> m_stopTimer{ nullptr };

    // must always be last
    std::jthread m_thread;

private:
    static constexpr size_t MAX_EVENTS_PER_LOOP{ 10 };
    static constexpr TimeMS PROCESS_EVENTS_THRESHOLD{ 1000ms };
    static constexpr TimeMS PROCESS_EVENTS_WAIT_TIMEOUT{ 100ms };
};

} // namespace Sage::Threading
