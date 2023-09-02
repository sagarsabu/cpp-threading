#pragma once

#include <unordered_map>

namespace Sage::Threading
{

// Event dispatching

enum class EventReceiverT
{
    Default,
    ThreadManager,
    ThreadWorker
};

struct ThreadEvent
{
    virtual ~ThreadEvent() = default;

    EventReceiverT Receiver() const { return m_receiver; }

    const char* ReceiverName() const
    {
        static const std::unordered_map<EventReceiverT, const char*> receiverNameMap
        {
            {EventReceiverT::Default,          "Default"},
            {EventReceiverT::ThreadManager,    "ThreadManager"},
            {EventReceiverT::ThreadWorker,     "ThreadWorker"},
        };

        auto itr = receiverNameMap.find(m_receiver);
        if (itr != receiverNameMap.end())
        {
            return itr->second;
        }

        return "Unknown";
    };

protected:
    explicit ThreadEvent(EventReceiverT receiver) : m_receiver{ receiver } { }

private:
    const EventReceiverT m_receiver;
};

// Events for the default receiver

enum class DefaultEventT
{
    Exit
};

struct DefaultEvent : public ThreadEvent
{
    virtual ~DefaultEvent() = default;

    DefaultEventT Type() const { return m_event; }

protected:
    explicit DefaultEvent(DefaultEventT eventType) :
        ThreadEvent{ EventReceiverT::Default },
        m_event{ eventType }
    { }

private:
    const DefaultEventT m_event;
};

struct ExitEvent final : public DefaultEvent
{
    ExitEvent() :
        DefaultEvent{ DefaultEventT::Exit }
    { }
};


} // namespace Sage::Threading

