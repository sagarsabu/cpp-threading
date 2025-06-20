#pragma once

#include <cstddef>
#include <deque>
#include <iterator>
#include <memory>
#include <mutex>
#include <semaphore>
#include <utility>

#include "log/logger.hpp"
#include "timers/timer.hpp"

namespace Sage::Channel
{

template<typename T> struct Notifier
{
    std::binary_semaphore m_notify{ 0 };
    std::recursive_mutex m_queueMtx{};
    std::deque<std::unique_ptr<T>> m_queue{};
    bool m_rxDisconnected{ true };
};

template<typename T> using SharedNotifier = std::shared_ptr<Notifier<T>>;

template<typename T> class Rx
{
public:
    explicit Rx(SharedNotifier<T> notifier) : m_notifier{ std::move(notifier) }
    {
        m_notifier->m_rxDisconnected = false;
    }

    ~Rx() { m_notifier->m_rxDisconnected = true; }

    std::unique_ptr<T> receive()
    {
        std::unique_ptr<T> res{ nullptr };
        std::binary_semaphore& sem{ m_notifier->m_notify };
        sem.acquire();

        {
            std::scoped_lock lk{ m_notifier->m_queueMtx };
            auto& queue{ m_notifier->m_queue };
            if (not queue.empty())
            {
                res = std::move(queue.front());
                queue.pop_front();
            }
        }

        return res;
    }

    std::deque<std::unique_ptr<T>> receiveMany()
    {
        std::deque<std::unique_ptr<T>> res;
        std::binary_semaphore& sem{ m_notifier->m_notify };
        sem.acquire();

        {
            std::scoped_lock lk{ m_notifier->m_queueMtx };
            std::swap(res, m_notifier->m_queue);
        }

        return res;
    }

    std::unique_ptr<T> tryReceive(const TimeNS& timeout)
    {
        std::unique_ptr<T> res{ nullptr };
        std::binary_semaphore& sem{ m_notifier->m_notify };
        if (not sem.try_acquire_for(timeout))
        {
            std::scoped_lock lk{ m_notifier->m_queueMtx };
            auto& queue{ m_notifier->m_queue };
            if (not queue.empty())
            {
                res = std::move(queue.front());
                queue.pop_front();
            }
        }

        return res;
    }

    std::deque<std::unique_ptr<T>> tryReceiveMany(const TimeNS& timeout)
    {
        std::deque<std::unique_ptr<T>> res;
        std::binary_semaphore& sem{ m_notifier->m_notify };

        if (sem.try_acquire_for(timeout))
        {
            std::scoped_lock lk{ m_notifier->m_queueMtx };
            std::swap(res, m_notifier->m_queue);
        }

        return res;
    }

    std::pair<std::deque<std::unique_ptr<T>>, size_t> tryReceiveLimitedMany(const TimeNS& timeout, size_t max)
    {
        std::deque<std::unique_ptr<T>> res;
        size_t leftInQueue{ 0 };
        std::binary_semaphore& sem{ m_notifier->m_notify };

        if (sem.try_acquire_for(timeout))
        {
            std::scoped_lock lk{ m_notifier->m_queueMtx };
            auto& queue{ m_notifier->m_queue };

            size_t queueSize{ queue.size() };
            size_t nEventsForThisPass{ std::min(queueSize, max) };
            if (queueSize > nEventsForThisPass)
            {
                leftInQueue = queueSize - nEventsForThisPass;
            }

            // steal as much as possible, but leave the rest in the queue
            auto stealEnd{ queue.begin() +
                           static_cast<decltype(m_notifier->m_queue)::difference_type>(nEventsForThisPass) };
            res.insert(res.end(), std::move_iterator(queue.begin()), std::move_iterator(stealEnd));
            // remove stolen events
            queue.erase(queue.begin(), stealEnd);
        }

        return std::make_pair(std::move(res), leftInQueue);
    }

    void wakeImmediately() { m_notifier->m_notify.release(); }

private:
    const SharedNotifier<T> m_notifier;
};

template<typename T> class Tx
{
public:
    explicit Tx(SharedNotifier<T> notifier) : m_notifier{ std::move(notifier) } {}

    ~Tx()
    {
        // notify on destruction, so any waiters are woken
        std::binary_semaphore& sem{ m_notifier->m_notify };
        sem.release();
    }

    void send(std::unique_ptr<T> t)
    {
        {
            std::scoped_lock lk{ m_notifier->m_queueMtx };
            if (m_notifier->m_rxDisconnected)
            {
                LOG_WARNING("rx disconnected on send");
                return;
            }

            auto& queue{ m_notifier->m_queue };
            queue.emplace_back(std::move(t));
        }

        std::binary_semaphore& sem{ m_notifier->m_notify };
        sem.release();
    }

    void flushAndSend(std::unique_ptr<T> t)
    {
        {
            std::scoped_lock lk{ m_notifier->m_queueMtx };
            if (m_notifier->m_rxDisconnected)
            {
                LOG_WARNING("rx disconnected on send");
                return;
            }

            auto& queue{ m_notifier->m_queue };
            queue.clear();
            queue.emplace_back(std::move(t));
        }

        std::binary_semaphore& sem{ m_notifier->m_notify };
        sem.release();
    }

private:
    const SharedNotifier<T> m_notifier;
};

template<typename T> struct ChannelPair
{
    std::shared_ptr<Tx<T>> tx;
    std::unique_ptr<Rx<T>> rx;
};

template<typename T> auto MakeChannel()
{
    auto notifier{ std::make_shared<Notifier<T>>() };
    auto rx{ std::make_unique<Rx<T>>(notifier) };
    auto tx{ std::make_shared<Tx<T>>(notifier) };
    return ChannelPair{ .tx = std::move(tx), .rx = std::move(rx) };
}

} // namespace Sage::Channel
