#include <cstring>
#include <memory>
#include <pthread.h>
#include <stop_token>
#include <thread>
#include <unordered_map>
#include <utility>

#include "channel/channel.hpp"
#include "log/logger.hpp"
#include "threading/events.hpp"
#include "timers/scoped_deadline.hpp"
#include "timers/timer_thread.hpp"
#include "uring/io_uring.hpp"

namespace Sage
{

TimerThread::TimerThread(Channel::ChannelPair<TimerEvent> channel) :
    m_tx{ std::move(channel.tx) },
    m_thread{ &TimerThread::Run, this, std::move(channel.rx) }
{
    LOG_DEBUG("timer thread c'tor");
}

TimerThread::~TimerThread() { LOG_DEBUG("timer thread d'tor"); }

TimerEventId TimerThread::RequestTimerAdd(const TimeNS& timeout, SharedThreadTx tx)
{
    auto e{ std::make_unique<TimerAddEvent>() };
    TimerEventId id{ e->m_id };
    e->m_timeout = timeout;
    e->m_tx = std::move(tx);
    LOG_DEBUG("requesting to add timer:{} with timeout:{}", id, timeout);

    m_tx->send(std::move(e));
    return id;
}

void TimerThread::RequestTimerUpdate(TimerEventId id, const TimeNS& timeout)
{
    LOG_DEBUG("requesting to update timer: {} to timeout:{}", id, timeout);

    auto e{ std::make_unique<TimerUpdateEvent>() };
    e->m_timerToUpdate = id;
    e->m_newTimeout = timeout;
    m_tx->send(std::move(e));
}

void TimerThread::RequestTimerStop(TimerEventId id, bool logOnDrop)
{
    LOG_DEBUG("requesting to stop timer:{}", id);

    auto e{ std::make_unique<TimerStopEvent>() };
    e->m_timerToStop = id;
    m_tx->send(std::move(e), logOnDrop);
}

void TimerThread::Run(std::unique_ptr<Channel::Rx<TimerEvent>> rx)
{
    pthread_setname_np(pthread_self(), "TimerThread");

    // For for start trigger
    m_startLatch.wait();

    LOG_INFO("timer thread started");

    std::stop_token stopToken{ m_thread.get_stop_token() };

    while (not stopToken.stop_requested())
    {
        if (auto uringEvent{ m_uring.WaitForEvent(20ms) }; uringEvent != nullptr)
        {
            URingEventId userData{ uringEvent->user_data };
            auto itr{ m_pendingUringEvents.find(userData) };
            if (itr == m_pendingUringEvents.end())
            {
                LOG_ERROR("failed to find event for user-data={}", userData);
                continue;
            }

            auto& event{ itr->second };

            event->m_onCompleteCb(*event, *uringEvent);

            // dont remove the continuously firing timer
            if (event->m_removeOnComplete)
            {
                m_pendingUringEvents.erase(itr);
            }
        }

        // for update ops
        auto channelEvents{ rx->tryReceiveMany(10ns) };
        for (const auto& e : channelEvents)
        {
            switch (e->Type())
            {
                case TimerEvent::Add:
                {
                    const auto& tEvent{ static_cast<const TimerAddEvent&>(*e) };
                    AddTimer(tEvent);
                    break;
                }

                case TimerEvent::Update:
                {
                    const auto& tEvent{ static_cast<const TimerUpdateEvent&>(*e) };
                    UpdateTimer(tEvent);
                    break;
                }

                case TimerEvent::Stop:
                {
                    const auto& tEvent{ static_cast<const TimerStopEvent&>(*e) };
                    CancelTimer(tEvent);
                    break;
                }
            }
        }
    }

    LOG_INFO("timer thread stopped");

    m_stopLatch.count_down();
}

// Queuing

void TimerThread::AddTimer(const TimerAddEvent& event)
{
    auto uringEvent{ std::make_unique<URingTimerExpiredEvent>(
        event.m_id,
        [this](URingTimerEvent& event, const io_uring_cqe& cEvent) { OnCompleteTimerExpired(event, cEvent); }
    ) };
    auto urId{ uringEvent->m_uringId };
    LOG_RETURN_IF(not m_uring.QueueTimeoutEvent(urId, event.m_timeout), LOG_CRITICAL);

    m_pendingUringEvents[urId] = std::move(uringEvent);
    m_txs[event.m_id] = event.m_tx;
    LOG_DEBUG("added timer id:{} timeout:{}", event.m_id, event.m_timeout);
}

void TimerThread::UpdateTimer(const TimerUpdateEvent& event)
{
    auto pendingEvent{ FindUringPendingEvent<URingTimerExpiredEvent>(event.m_timerToUpdate) };
    LOG_RETURN_IF(pendingEvent == nullptr, LOG_CRITICAL);

    auto uringEvent{ std::make_unique<URingTimerUpdateEvent>(
        event.m_id, [this](URingTimerEvent& event, const io_uring_cqe& cEvent) { OnCompleteTimerUpdate(event, cEvent); }
    ) };
    auto urId{ uringEvent->m_uringId };
    LOG_RETURN_IF(not m_uring.UpdateTimeoutEvent(urId, pendingEvent->m_uringId, event.m_newTimeout), LOG_CRITICAL);

    m_pendingUringEvents[urId] = std::move(uringEvent);
    LOG_DEBUG("updated timer id:{} timeout:{}", event.m_id, event.m_newTimeout);
}

void TimerThread::CancelTimer(const TimerStopEvent& event)
{
    auto pendingEvent{ FindUringPendingEvent<URingTimerExpiredEvent>(event.m_timerToStop) };
    LOG_RETURN_IF(pendingEvent == nullptr, LOG_CRITICAL);

    auto uringEvent{ std::make_unique<URingTimerCancelEvent>(
        event.m_id, [this](URingTimerEvent& event, const io_uring_cqe& cEvent) { OnCompleteTimerCancel(event, cEvent); }
    ) };
    auto urId{ uringEvent->m_uringId };
    LOG_RETURN_IF(not m_uring.CancelTimeoutEvent(urId, pendingEvent->m_uringId), LOG_CRITICAL);

    m_pendingUringEvents[urId] = std::move(uringEvent);
    LOG_DEBUG("cancelled timer id:{}", event.m_id);
}

// Callbacks

void TimerThread::OnCompleteTimerExpired(URingTimerEvent& event, const io_uring_cqe& cEvent)
{
    int eventRes{ cEvent.res };

    auto itr{ m_txs.find(event.m_timerEventId) };
    LOG_RETURN_IF(itr == m_txs.end(), LOG_CRITICAL);

    switch (eventRes)
    {
        // timer expired
        case -ETIME:
        {
            LOG_DEBUG("triggering handler eventId({})", event.m_timerEventId);

            {
                ScopedDeadline dl{ "CompleteTimerExpiredEvent:" + std::to_string(event.m_timerEventId), 20ms };
                auto& tx{ itr->second };
                tx->send(std::make_unique<TimerExpiredEvent>(event.m_timerEventId));
            }

            break;
        }

        // timer cancelled
        case -ECANCELED:
        {
            LOG_DEBUG("timer cancelled eventId({})", event.m_timerEventId);
            m_txs.erase(itr);
            break;
        }

        default:
        {
            LOG_ERROR("failed eventId({}) res({}) {}", event.m_timerEventId, eventRes, strerror(-eventRes));
            break;
        }
    }
}

void TimerThread::OnCompleteTimerUpdate(URingTimerEvent& event, const io_uring_cqe& cEvent)
{
    int eventRes{ cEvent.res };
    switch (eventRes)
    {
        // timer update acknowledged
        case 0:
        {
            LOG_DEBUG("timer update acknowledged eventId({})", event.m_timerEventId);
            break;
        }

        default:
        {
            LOG_ERROR("failed eventId({}) res({}) {}", event.m_timerEventId, eventRes, strerror(-eventRes));
            break;
        }
    }
}

void TimerThread::OnCompleteTimerCancel(URingTimerEvent& event, const io_uring_cqe& cEvent)
{
    int eventRes{ cEvent.res };
    switch (eventRes)
    {
        // timer cancellation acknowledged
        case 0:
        {
            LOG_DEBUG("timer cancellation acknowledged eventId({})", event.m_timerEventId);
            break;
        }

        default:
        {
            LOG_ERROR("failed eventId({}) res({}) {}", event.m_timerEventId, eventRes, strerror(-eventRes));
            break;
        }
    }
}

} // namespace Sage
