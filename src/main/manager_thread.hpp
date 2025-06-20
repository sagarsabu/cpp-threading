#pragma once

#include <atomic>
#include <semaphore>
#include <set>

#include "threading/events.hpp"
#include "threading/thread.hpp"
#include "timers/time_utils.hpp"

namespace Sage
{

// Manager events

class ManagerEvent : public ThreadEvent
{
public:
    enum Event
    {
        Shutdown,
        WorkerTest
    };

    virtual ~ManagerEvent() override = default;

    Event Type() const { return m_event; }

protected:
    ManagerEvent(Event eventType, EventReceiver receiver) : ThreadEvent{ receiver }, m_event{ eventType } {}

private:
    const Event m_event;
};

class ManagerShutdownEvent final : public ManagerEvent
{
public:
    ManagerShutdownEvent() : ManagerEvent{ Event::Shutdown, EventReceiver::ManagerThread } {}
};

class ManagerWorkerTestEvent final : public ManagerEvent
{
public:
    explicit ManagerWorkerTestEvent(const TimeMS& timeout) :
        ManagerEvent{ Event::WorkerTest, EventReceiver::WorkerThread },
        m_timeout{ timeout }
    {
    }

    TimeMS m_timeout;
};

// Manager thread

class ManagerThread final : public Thread
{
public:
    static constexpr inline TimeMS TEARDOWN_THRESHOLD{ 1000ms };
    static constexpr inline TimeMS DEFAULT_TEST_TIMEOUT{ 10ms };
    static constexpr inline TimeMS DEFAULT_TRANSMIT_PERIOD{ 15ms };

public:
    explicit ManagerThread(TimerThread& timerThread);

    void AttachWorker(Thread* worker);

    void RequestShutdown();

    void WaitForShutdown();

    void SetTransmitPeriod(const TimeMS& period) { m_transmitPeriod = period; }

private:
    void SendEventsToWorkers();

    void TeardownWorkers();

    void TryWaitForWorkersShutdown();

    void TryWaitForManagerShutdown();

    void InitiateShutdown();

    bool WorkersRunning();

    void Starting() override;

    void HandleEvent(UniqueThreadEvent event) override;

private:
    std::set<Thread*> m_workers{};
    std::mutex m_workersMtx{};
    std::atomic<bool> m_workersTerminated{ false };
    std::binary_semaphore m_shutdownInitiateSignal{ 0 };
    std::binary_semaphore m_shutdownInitiatedSignal{ 0 };
    TimerEventId m_transmitTimerId{ 0 };
    TimeMS m_transmitPeriod{ DEFAULT_TRANSMIT_PERIOD };
    TimeMS m_testTimeout{ DEFAULT_TEST_TIMEOUT };
};

} // namespace Sage
