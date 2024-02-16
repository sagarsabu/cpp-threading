#pragma once

#include <chrono>

#include "log/logger.hpp"
#include "timers/timer.hpp"

namespace Sage::Threading
{

struct ScopedDeadline final
{
    ScopedDeadline(const std::string& tag, const TimeMilliSec& deadline) :
        m_tag{ tag },
        m_deadline{ deadline }
    { }

    ~ScopedDeadline()
    {
        auto now = Clock::now();
        auto duration = std::chrono::duration_cast<TimeMilliSec>(now - m_start);
        if (duration <= m_deadline)
        {
            Log::Trace("ScopedDeadline '%s' took:%ld ms", m_tag.c_str(), duration.count());
        }
        else
        {
            Log::Warning("ScopedDeadline '%s' took:%ld ms deadline:%ld ms",
                m_tag.c_str(), duration.count(), m_deadline.count());
        }
    }

private:
    const Clock::time_point m_start{ Clock::now() };
    const std::string m_tag;
    const TimeMilliSec m_deadline;
};

} // namespace Sage::Threading
