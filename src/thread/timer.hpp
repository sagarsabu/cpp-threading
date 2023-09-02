#pragma once

#include <chrono>

#include "log/logger.hpp"

namespace Sage::Threading
{

struct ScopeTimer final
{
    explicit ScopeTimer(const std::string& tag) :
        m_start{ std::chrono::high_resolution_clock::now() },
        m_tag{ tag }
    { }

    ~ScopeTimer()
    {
        auto now = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_start);
        Log::Debug("ScopedTimer '%s' took:%ld ms", m_tag.c_str(), duration.count());
    }

private:
    const std::chrono::high_resolution_clock::time_point m_start;
    const std::string m_tag;
};

} // namespace Sage::Threading
