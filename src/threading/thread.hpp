#pragma once

#include <thread>
#include <queue>
#include <memory>
#include <string>
#include <mutex>
#include <semaphore>
#include <chrono>
#include <atomic>

#include "threading/timer.hpp"

namespace Sage::Threading
{

// Forward declaration

class ThreadEvent;

// Aliases

using UniqueThreadEvent = std::unique_ptr<ThreadEvent>;

class Thread
{
public:
    explicit Thread(const std::string& threadName);

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

private:
    // thread entry point
    void Enter();

    // main thread loop
    int Execute();

    void ProcessEvents(const TimeMilliSec& timeout = 1000ms);

    // For loop back events for managing this thread
    void HandleSelfEvent(UniqueThreadEvent event);

private:
    const std::string m_threadName;
    std::mutex m_threadCreationMtx;
    std::jthread m_thread;
    std::mutex m_eventQueueMtx;
    std::queue<UniqueThreadEvent> m_eventQueue;
    std::binary_semaphore m_eventSignal;
    std::atomic<int> m_exitCode;
    std::atomic<bool> m_running;
    std::atomic<bool> m_stopping;
    std::unique_ptr<FireOnceTimer> m_stopTimer;

private:
    static constexpr size_t MAX_EVENTS_PER_LOOP{ 10 };
};

} // namespace Sage::Threading
