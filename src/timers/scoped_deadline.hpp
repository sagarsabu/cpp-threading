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
            LOG_TRACE("ScopedDeadline '{}' took:{}", m_tag, duration);
        }
        else
        {
            LOG_WARNING("ScopedDeadline '{}' took:{} deadline:{}",
                m_tag, duration, m_deadline);
        }
    }

private:
    const Clock::time_point m_start{ Clock::now() };
    const std::string m_tag;
    const TimeMS m_deadline;
};

} // namespace Sage
