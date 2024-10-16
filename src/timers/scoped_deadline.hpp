#pragma once

#include <chrono>

#include "log/logger.hpp"
#include "timers/timer.hpp"

namespace Sage
{

struct ScopedDeadline final
{
    ScopedDeadline(const std::string& tag, const TimeMS& deadline) :
        m_tag{ tag },
        m_deadline{ deadline }
    { }

    ~ScopedDeadline()
    {
        auto now = Clock::now();
        auto duration = std::chrono::duration_cast<TimeMS>(now - m_start);
        if (duration <= m_deadline)
        {
            LOG_TRACE("ScopedDeadline '%s' took:%ld ms", m_tag.c_str(), duration.count());
        }
        else
        {
            LOG_WARNING("ScopedDeadline '%s' took:%ld ms deadline:%ld ms",
                m_tag.c_str(), duration.count(), m_deadline.count());
        }
    }

private:
    const Clock::time_point m_start{ Clock::now() };
    const std::string m_tag;
    const TimeMS m_deadline;
};

} // namespace Sage
