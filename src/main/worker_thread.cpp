#include "log/logger.hpp"
#include "main/worker_thread.hpp"
#include "main/manager_thread.hpp"

namespace Sage::Threading
{

// Worker thread

WorkerThread::WorkerThread() :
    Thread{ std::string("WkrThread-") + std::to_string(++s_id) }
{ }

void WorkerThread::HandleEvent(UniqueThreadEvent threadEvent)
{
    if (threadEvent->Receiver() != EventReceiver::WorkerThread) [[unlikely]]
    {
        LOG_ERROR("{} handle-event got event intended for receiver:{}",
            Name(), threadEvent->ReceiverName());
        return;
    }

    auto& event = static_cast<ManagerEvent&>(*threadEvent);
    switch (event.Type())
    {
        case ManagerEvent::WorkerTest:
        {
            auto& rxEvent = static_cast<ManagerWorkerTestEvent&>(event);
            LOG_INFO("{} handle-event 'Test'. sleeping for {}",
                Name(), rxEvent.m_timeout);
            std::this_thread::sleep_for(rxEvent.m_timeout);
            break;
        }

        default:
        {
            LOG_ERROR("{} handle-event unknown event:{}",
                Name(), static_cast<int>(event.Type()));
            break;
        }
    }
}

} // namespace Sage::Threading
