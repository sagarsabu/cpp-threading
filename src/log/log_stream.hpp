#pragma once

#include <string>
#include <iostream>
#include <fstream>
#include <mutex>

#include "timers/timer.hpp"
#include "log/log_levels.hpp"

namespace Sage::Logger::Internal
{

class LogStreamer
{
public:
    using Stream = std::ostream;

    LogStreamer() = default;

    void Setup(const std::string& filename, Level level);

    Level GetLogLevel() const noexcept { return m_logLevel; }

private:
    // Nothing in here is movable or copyable
    LogStreamer(const LogStreamer&) = delete;
    LogStreamer(LogStreamer&&) = delete;
    LogStreamer& operator=(const LogStreamer&) = delete;
    LogStreamer& operator=(LogStreamer&&) = delete;

    void EnsureLogFileWriteable();

    void SetStreamToConsole();

    void SetStreamToFile(std::ofstream fileStream);

private:
    static constexpr TimeS s_logFileCreatorPeriod{ 60s };
    static constexpr std::reference_wrapper<Stream> s_consoleStream{ std::cout };

    std::reference_wrapper<Stream> m_streamRef{ s_consoleStream };
    std::recursive_mutex m_mutex{};
    std::string m_logFilename{};
    Level m_logLevel{ Level::Info };
    size_t m_lostLogTime{ 0 };
    std::ofstream m_logFileStream{};
    std::unique_ptr<PeriodicTimer> m_logFileCreator{ nullptr };

    template<typename ...Args>
    friend inline void LogToStream(Level level, std::format_string<Args...> fmt, Args&&... args);
};

LogStreamer& GetLogStreamer() noexcept;


} // namespace Sage::Logger::Internal


