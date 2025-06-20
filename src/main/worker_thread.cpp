#include "main/worker_thread.hpp"
#include "log/logger.hpp"
#include "main/manager_thread.hpp"

namespace Sage
{

// Worker thread

WorkerThread::WorkerThread(TimerThread& timerThread) :
    Thread{ std::string("WkrThread-") + std::to_string(++s_id), timerThread }
{
}

void WorkerThread::HandleEvent(UniqueThreadEvent threadEvent)
{
    LOG_RETURN_IF(threadEvent->Receiver() != EventReceiver::WorkerThread, LOG_ERROR);

    auto& event = static_cast<ManagerEvent&>(*threadEvent);
    switch (event.Type())
    {
        case ManagerEvent::WorkerTest:
        {
            auto& rxEvent = static_cast<ManagerWorkerTestEvent&>(event);
            LOG_INFO("{} handle-event 'Test'. sleeping for {}", Name(), rxEvent.m_timeout);
            std::this_thread::sleep_for(rxEvent.m_timeout);
            break;
        }

        default:
        {
            LOG_ERROR("{} handle-event unknown event:{}", Name(), static_cast<int>(event.Type()));
            break;
        }
    }
}

} // namespace Sage
