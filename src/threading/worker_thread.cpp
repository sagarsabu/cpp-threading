#include "log/logger.hpp"
#include "threading/worker_thread.hpp"
#include "threading/manager_thread.hpp"

namespace Sage::Threading
{

// Worker thread

WorkerThread::WorkerThread() :
    Thread{ std::string("WkrThread-") + std::to_string(++s_id) }
{ }

void WorkerThread::HandleEvent(UniqueThreadEvent threadEvent)
{
    if (threadEvent->Receiver() != EventReceiver::ManagerThread)
    {
        Log::Error("%s handle-event got event from unexpected receiver:%s",
            Name(), threadEvent->ReceiverName());
        return;
    }

    auto& event = static_cast<ManagerEvent&>(*threadEvent);
    switch (event.Type())
    {
        case ManagerEvent::WorkerTest:
        {
            auto& rxEvent = static_cast<ManagerWorkerTestEvent&>(event);
            Log::Info("%s handle-event 'Test'. sleeping for %ld ms",
                Name(), rxEvent.m_timeout.count());
            std::this_thread::sleep_for(rxEvent.m_timeout);
            break;
        }

        default:
        {
            Log::Error("%s handle-event unknown event:%d",
                Name(), static_cast<int>(event.Type()));
            break;
        }
    }
}

} // namespace Sage::Threading
