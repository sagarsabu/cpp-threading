#include <cerrno>
#include <cstring>
#include <liburing.h>
#include <netdb.h>
#include <sys/socket.h>

#include "log/logger.hpp"
#include "uring/io_uring.hpp"

namespace Sage
{

IOURing::IOURing(uint queueSize) : m_queueSize{ queueSize } { io_uring_queue_init(m_queueSize, &m_rawIOURing, 0); }

IOURing::~IOURing() { io_uring_queue_exit(&m_rawIOURing); }

UniqueUringCEvent IOURing::WaitForEvent(const TimeNS& timeout)
{
    LOG_TRACE("Waiting for events to populate");

    io_uring_cqe* rawCEvent{ nullptr };
    __kernel_timespec ts{ ChronoTimeToKernelTimeSpec(timeout) };
    if (int res = io_uring_wait_cqe_timeout(&m_rawIOURing, &rawCEvent, &ts); res < 0)
    {
        switch (res)
        {
            // Ignore interrupts. i.e debugger pause / suspend
            case -EINTR:
            // timedout
            case -ETIME:
                break;

            default:
            {
                LOG_ERROR("failed to waiting for event completion. {}", strerror(-res));
                break;
            }
        }

        return nullptr;
    }

    return UniqueUringCEvent{ rawCEvent,
                              [this](io_uring_cqe* event)
                              {
                                  if (event != nullptr)
                                  {
                                      io_uring_cqe_seen(&m_rawIOURing, event);
                                  }
                              } };
}

bool IOURing::QueueTimeoutEvent(const UserData& data, const TimeNS& timeout)
{
    io_uring_sqe* submissionEvent{ GetSubmissionEvent() };
    LOG_RETURN_FALSE_IF(submissionEvent == nullptr, LOG_CRITICAL);

    submissionEvent->user_data = data;
    __kernel_timespec ts{ ChronoTimeToKernelTimeSpec(timeout) };
    io_uring_prep_timeout(
        submissionEvent,
        &ts,
        0,
        // ensure timeout keeps firing without rearming
        IORING_TIMEOUT_MULTISHOT | IORING_TIMEOUT_BOOTTIME
    );

    return SubmitEvents();
}

bool IOURing::CancelTimeoutEvent(const UserData& cancelData, const UserData& timeoutData)
{
    io_uring_sqe* submissionEvent{ GetSubmissionEvent() };
    LOG_RETURN_FALSE_IF(submissionEvent == nullptr, LOG_CRITICAL);

    submissionEvent->user_data = cancelData;
    io_uring_prep_timeout_remove(submissionEvent, timeoutData, 0);

    return SubmitEvents();
}

bool IOURing::UpdateTimeoutEvent(const UserData& updateData, const UserData& timeoutData, const TimeNS& timeout)
{
    io_uring_sqe* submissionEvent{ GetSubmissionEvent() };
    LOG_RETURN_FALSE_IF(submissionEvent == nullptr, LOG_CRITICAL);

    submissionEvent->user_data = updateData;
    __kernel_timespec ts{ ChronoTimeToKernelTimeSpec(timeout) };
    io_uring_prep_timeout_update(submissionEvent, &ts, timeoutData, 0);

    return SubmitEvents();
}

bool IOURing::SubmitEvents()
{
    int res{ io_uring_submit(&m_rawIOURing) };
    bool success{ res >= 0 };

    if (success) [[likely]]
    {
        LOG_TRACE("submitted {} event(s)", res);
    }
    else
    {
        LOG_ERROR("failed. {}", strerror(-res));
    }

    return success;
}

io_uring_sqe* IOURing::GetSubmissionEvent()
{
    io_uring_sqe* submissionEvent{ io_uring_get_sqe(&m_rawIOURing) };
    LOG_IF(submissionEvent == nullptr, LOG_CRITICAL);
    return submissionEvent;
}

} // namespace Sage
