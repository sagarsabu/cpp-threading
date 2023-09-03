#pragma once

#include <chrono>

#include "log/logger.hpp"
#include "threading/timer.hpp"

namespace Sage::Threading
{

struct ScopedTimer final
{
    explicit ScopedTimer(const std::string& tag) :
        m_start{ Clock::now() },
        m_tag{ tag }
    { }

    ~ScopedTimer()
    {
        auto now = Clock::now();
        auto duration = std::chrono::duration_cast<TimeMilliSec>(now - m_start);
        Log<Debug>("ScopedTimer '%s' took:%ld ms", m_tag.c_str(), duration.count());
    }

private:
    const Clock::time_point m_start;
    const std::string m_tag;
};

} // namespace Sage::Threading
