#pragma once

#include <thread>
#include <queue>
#include <memory>
#include <string>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <atomic>


namespace Sage::Thread
{

using TimerMS = std::chrono::milliseconds;

class Event;

const char* GetThreadId();

class ThreadI
{
public:
    explicit ThreadI(const std::string& threadName);

    virtual ~ThreadI();

    const char* Name() const { return m_threadName.c_str(); }

    void Start();

    void Stop();

    void TransmitEvent(std::unique_ptr<Event> event);

    int ExitCode() const { return m_exitCode; }

    bool IsRunning() const { return m_running; }

protected:
    virtual void Starting() { }

    // main thread loop
    virtual int Execute();

    virtual void Stopping() { }

    std::unique_ptr<Event> WaitForEvent(const TimerMS& timeout = TimerMS{ 1000 });

    virtual void HandleEvent(std::unique_ptr<Event> /**event*/) { };

private:
    void FlushEvents();

    // thread entry point
    void Enter();

private:
    const std::string m_threadName;
    std::mutex m_threadCreationMtx;
    std::thread m_thread;
    std::mutex m_eventQueueMtx;
    std::condition_variable m_eventQueueCndVar;
    std::queue<std::unique_ptr<Event>> m_eventQueue;
    std::atomic<bool> m_running;
    std::atomic<int> m_exitCode;

private:
    static constexpr size_t MAX_EVENTS_PER_POLL{ 10 };
};

} // namespace Sage::Thread
