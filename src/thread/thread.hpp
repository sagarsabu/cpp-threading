#pragma once

#include <thread>
#include <queue>
#include <memory>
#include <string>
#include <mutex>
#include <semaphore>
#include <chrono>
#include <atomic>

namespace Sage::Threading
{

// Forward declaration

struct ThreadEvent;

// Aliases

using TimeMS = std::chrono::milliseconds;
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

    // main thread loop
    virtual int Execute();

    virtual void Stopping() { }

    UniqueThreadEvent WaitForEvent(const TimeMS& timeout = TimeMS{ 1000 });

    virtual void HandleEvent(UniqueThreadEvent event);

private:
    // thread entry point
    void Enter();

    void FlushEvents();

private:
    const std::string m_threadName;
    std::thread m_thread;
    std::mutex m_eventQueueMtx;
    std::queue<UniqueThreadEvent> m_eventQueue;
    std::binary_semaphore m_eventSignal;
    std::atomic<bool> m_started;
    std::atomic<bool> m_running;
    std::atomic<bool> m_stoped;
    std::atomic<int> m_exitCode;

private:
    static constexpr size_t MAX_EVENTS_PER_LOOP{ 10 };
};

} // namespace Sage::Threading
