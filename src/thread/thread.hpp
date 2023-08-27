#pragma once

#include <thread>
#include <queue>
#include <memory>
#include <string>
#include <mutex>
#include <semaphore>
#include <chrono>
#include <atomic>

namespace Sage::Thread
{

// Forward declaration

class Event;

const char* GetThreadId();

// Aliases

using TimerMS = std::chrono::milliseconds;
using ThreadEvent = std::unique_ptr<Event>;

class ThreadI
{
public:
    explicit ThreadI(const std::string& threadName);

    virtual ~ThreadI();

    const char* Name() const { return m_threadName.c_str(); }

    void Start();

    void Stop();

    void TransmitEvent(ThreadEvent event);

    int ExitCode() const { return m_exitCode; }

    bool IsRunning() const { return m_running; }

protected:
    virtual void Starting() { }

    // main thread loop
    virtual int Execute();

    virtual void Stopping() { }

    ThreadEvent WaitForEvent(const TimerMS& timeout = TimerMS{ 1000 });

    virtual void HandleEvent(ThreadEvent /**event*/) { };

private:
    // thread entry point
    void Enter();

    void FlushEvents();

private:
    const std::string m_threadName;
    std::mutex m_threadCreationMtx;
    std::thread m_thread;
    std::mutex m_eventQueueMtx;
    std::queue<ThreadEvent> m_eventQueue;
    std::binary_semaphore m_eventQueueSmp;
    std::atomic<bool> m_running;
    std::atomic<int> m_exitCode;

private:
    static constexpr size_t MAX_EVENTS_PER_LOOP{ 10 };
};

} // namespace Sage::Thread
