#pragma once

#include <thread>
#include <queue>
#include <memory>
#include <string>
#include <mutex>
#include <semaphore>
#include <atomic>
#include <latch>
#include <unordered_map>

#include "threading/events.hpp"
#include "threading/timer.hpp"

namespace Sage::Threading
{

// Aliases

using UniqueThreadEvent = std::unique_ptr<ThreadEvent>;

class Thread
{
public:
    Thread(const std::string& threadName, const TimeMilliSec& handleEventThreshold = 20ms);

    virtual ~Thread();

    const char* Name() const { return m_threadName.c_str(); }

    void Start();

    void Stop();

    void TransmitEvent(UniqueThreadEvent event);

    int ExitCode() const { return m_exitCode; }

    bool IsRunning() const { return m_running; }

protected:
    virtual void Starting() { }

    virtual void Stopping() { }

    virtual void HandleEvent(UniqueThreadEvent event);

    void AddPeriodicTimer(TimerEvent::EventID timerEventId, TimeMilliSec period);

    void AddFireOnceTimer(TimerEvent::EventID timerEventId, TimeMilliSec delta);

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
    void Enter();

    // main thread loop
    int Execute();

    void ProcessEvents(const TimeMilliSec& timeout = 1000ms);

    // For loop back events for managing this thread
    void HandleSelfEvent(UniqueThreadEvent event);

private:
    const std::string m_threadName;
    std::jthread m_thread;
    std::mutex m_eventQueueMtx;
    std::queue<UniqueThreadEvent> m_eventQueue;
    std::binary_semaphore m_eventSignal;
    std::atomic<int> m_exitCode;
    std::latch m_startLatch;
    std::atomic<bool> m_running;
    std::atomic<bool> m_stopping;
    std::unique_ptr<FireOnceTimer> m_stopTimer;
    std::unordered_map<TimerEvent::EventID, std::unique_ptr<Timer>> m_timerEvents;
    const TimeMilliSec m_handleEventThreshold;

private:
    static constexpr size_t MAX_EVENTS_PER_LOOP{ 10 };
};

} // namespace Sage::Threading
