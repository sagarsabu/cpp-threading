#include "log/logger.hpp"
#include "threading/worker_thread.hpp"

namespace Sage::Threading
{

// Worker thread

WorkerThread::WorkerThread() :
    Thread{ std::string("WkrThread-") + std::to_string(++s_id) }
{ }

void WorkerThread::HandleEvent(UniqueThreadEvent threadEvent)
{
    if (threadEvent->Receiver() != EventReceiver::WorkerThread)
    {
        Log::Error("%s handle-event got event for expected receiver:%s",
            Name(), threadEvent->ReceiverName());
        return;
    }

    auto& event = static_cast<WorkerEvent&>(*threadEvent);
    switch (event.Type())
    {
        case WorkerEvent::Test:
        {
            auto& rxEvent = static_cast<WorkerTestEvent&>(event);
            Log::Info("%s handle-event 'Test'. sleeping for %ld ms",
                Name(), rxEvent.m_timeout.count());
            std::this_thread::sleep_for(rxEvent.m_timeout);
            break;
        }

        default:
            Log::Error("%s handle-event unknown event:%d",
                Name(), static_cast<int>(event.Type()));
            break;
    }
}

} // namespace Sage::Threading
