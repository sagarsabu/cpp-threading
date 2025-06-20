#pragma once

#include <atomic>
#include <latch>
#include <memory>
#include <thread>
#include <unistd.h>
#include <unordered_map>

#include "channel/channel.hpp"
#include "threading/events.hpp"
#include "timers/time_utils.hpp"
#include "uring/io_uring.hpp"

namespace Sage
{

using URingEventId = size_t;

class URingTimerEvent
{
public:
    using OnCompleteFunc = std::move_only_function<void(URingTimerEvent&, const io_uring_cqe&)>;

    virtual ~URingTimerEvent() noexcept = default;

    const URingEventId m_uringId{ s_nextId.fetch_add(1) };
    const TimerEventId m_timerEventId;
    OnCompleteFunc m_onCompleteCb;
    bool m_removeOnComplete{ true };

protected:
    URingTimerEvent(TimerEventId timerId, OnCompleteFunc&& onComplete) :
        m_timerEventId{ timerId },
        m_onCompleteCb{ std::move(onComplete) }
    {
    }

private:
    URingTimerEvent() = delete;

    static inline std::atomic<URingEventId> s_nextId{ 0 };
};

class URingTimerExpiredEvent final : public URingTimerEvent
{
public:
    URingTimerExpiredEvent(TimerEventId timerId, OnCompleteFunc&& onComplete) :
        URingTimerEvent{ timerId, std::move(onComplete) }
    {
        m_removeOnComplete = false;
    }
};

class URingTimerUpdateEvent final : public URingTimerEvent
{
public:
    URingTimerUpdateEvent(TimerEventId timerId, OnCompleteFunc&& onComplete) :
        URingTimerEvent{ timerId, std::move(onComplete) }
    {
    }
};

class URingTimerCancelEvent final : public URingTimerEvent
{
public:
    URingTimerCancelEvent(TimerEventId timerId, OnCompleteFunc&& onComplete) :
        URingTimerEvent{ timerId, std::move(onComplete) }
    {
    }
};

class TimerThread
{
public:
    using SharedThreadTx = std::shared_ptr<Channel::Tx<ThreadEvent>>;

    explicit TimerThread(Channel::ChannelPair<TimerEvent> channel = Channel::MakeChannel<TimerEvent>());

    ~TimerThread();

    void Start() { m_startLatch.count_down(); }

    TimerEventId RequestTimerAdd(const TimeNS& timeout, SharedThreadTx tx);

    void RequestTimerUpdate(TimerEventId, const TimeNS& timeout);

    void RequestTimerStop(TimerEventId);

private:
    TimerThread(const TimerThread&) = delete;
    TimerThread(TimerThread&&) = delete;
    TimerThread& operator=(const TimerThread&) = delete;
    TimerThread& operator=(TimerThread&&) = delete;

    void Run(std::unique_ptr<Channel::Rx<TimerEvent>> rx);

    template<typename ET> auto FindUringPendingEvent(TimerEventId id)
    {
        ET* res{ nullptr };
        for (auto& [_, pending] : m_pendingUringEvents)
        {
            auto child{ dynamic_cast<ET*>(pending.get()) };
            if (child == nullptr)
            {
                continue;
            }

            if (pending->m_timerEventId != id)
            {
                continue;
            }

            res = child;
            break;
        }

        return res;
    }

    void AddTimer(const TimerAddEvent&);
    void UpdateTimer(const TimerUpdateEvent&);
    void CancelTimer(const TimerStopEvent&);

    void OnCompleteTimerExpired(URingTimerEvent& event, const io_uring_cqe& cEvent);
    void OnCompleteTimerUpdate(URingTimerEvent& event, const io_uring_cqe& cEvent);
    void OnCompleteTimerCancel(URingTimerEvent& event, const io_uring_cqe& cEvent);

private:
    IOURing m_uring{ 10'000 };
    std::unordered_map<URingEventId, std::unique_ptr<URingTimerEvent>> m_pendingUringEvents;
    std::unordered_map<TimerEventId, SharedThreadTx> m_txs;
    std::shared_ptr<Channel::Tx<TimerEvent>> m_tx;
    std::latch m_startLatch{ 1 };
    std::jthread m_thread;
};

} // namespace Sage
