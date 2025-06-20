#pragma once

#include <functional>
#include <liburing.h>
#include <memory>
#include <sys/types.h>

#include "timers/time_utils.hpp"

namespace Sage
{

using UniqueUringCEvent = std::unique_ptr<io_uring_cqe, std::function<void(io_uring_cqe*)>>;

class IOURing final
{
public:
    // usually an id to reference against a map
    using UserData = decltype(io_uring_sqe{}.user_data);

    explicit IOURing(uint queueSize);

    ~IOURing();

    UniqueUringCEvent WaitForEvent(const TimeNS& timeout = 100ms);

    bool QueueTimeoutEvent(const UserData& data, const TimeNS& timeout);

    bool CancelTimeoutEvent(const UserData& cancelData, const UserData& timeoutData);

    bool UpdateTimeoutEvent(const UserData& cancelData, const UserData& timeoutData, const TimeNS& timeout);

private:
    IOURing(const IOURing&) = delete;
    IOURing(IOURing&&) = delete;
    IOURing& operator=(const IOURing&) = delete;
    IOURing& operator=(IOURing&&) = delete;

    io_uring_sqe* GetSubmissionEvent();

    bool SubmitEvents();

    io_uring m_rawIOURing{};

    const uint m_queueSize;
};

} // namespace Sage
