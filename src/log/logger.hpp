#pragma once

#include <string>
#include <format>
#include <print>
#include <mutex>
#include <iostream>
#include <fstream>

#include "timers/timer.hpp"

namespace Sage
{

namespace Logger
{

enum Level
{
    Trace,
    Debug,
    Info,
    Warning,
    Error,
    Critical
};

void SetupLogger(const std::string& filename = "", Level logLevel = Level::Info);

namespace Internal
{

std::string_view GetLevelFormatter(Level level) noexcept;

std::string_view GetLevelName(Level level) noexcept;

std::string_view GetFormatEnd() noexcept;

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

struct LogTimestamp
{
    // e.g "01 - 09 - 2023 00:42 : 19"
    using SecondsBuffer = char[26];
    // :%09lu requires 22 bytes max
    using NanoSecBuffer = char[22];

    LogTimestamp() noexcept;

    constexpr const SecondsBuffer& getSecondsBuffer() const noexcept { return m_secondsBuffer; };

    constexpr const NanoSecBuffer& getMilliSecBuffer() const noexcept { return m_extraSecBuff; };

private:
    SecondsBuffer m_secondsBuffer;
    NanoSecBuffer m_extraSecBuff;
    timespec m_timeSpec;
};

std::string_view CurrentThreadName() noexcept;

LogStreamer& GetLogStreamer() noexcept;

bool ShouldLog(Level level) noexcept;

template<typename ...Args>
[[gnu::always_inline]] inline void LogToStream(Level level, std::format_string<Args...> fmt, Args&&... args)
{
    if (not Internal::ShouldLog(level))
        return;

    LogTimestamp ts;

    {
        auto& logStreamer{ GetLogStreamer() };
        std::lock_guard lock{ logStreamer.m_mutex };
        LogStreamer::Stream& stream{ logStreamer.m_streamRef.get() };
        std::println(
            stream,
            "{}[{}{}] [{}] [{}] {}{}",
            GetLevelFormatter(level),
            ts.getSecondsBuffer(), ts.getMilliSecBuffer(),
            CurrentThreadName(),
            GetLevelName(level),
            std::format(fmt, std::forward_like<Args>(args)...),
            GetFormatEnd()
        );
        std::flush(stream);
    }
}

template<typename ...Args>
[[gnu::always_inline]] inline void Trace(std::format_string<Args...> fmt, Args&&... args)
{
    Logger::Internal::LogToStream(Logger::Trace, fmt, std::forward_like<Args>(args)...);
}

template<typename ...Args>
[[gnu::always_inline]] inline void Debug(std::format_string<Args...> fmt, Args&&... args)
{
    Logger::Internal::LogToStream(Logger::Debug, fmt, std::forward_like<Args>(args)...);
}

template<typename ...Args>
[[gnu::always_inline]] inline void Info(std::format_string<Args...> fmt, Args&&... args)
{
    Logger::Internal::LogToStream(Logger::Info, fmt, std::forward_like<Args>(args)...);
}

template<typename ...Args>
[[gnu::always_inline]] inline void Warning(std::format_string<Args...> fmt, Args&&... args)
{
    Logger::Internal::LogToStream(Logger::Warning, fmt, std::forward_like<Args>(args)...);
}

template<typename ...Args>
[[gnu::always_inline]] inline void Error(std::format_string<Args...> fmt, Args&&... args)
{
    Logger::Internal::LogToStream(Logger::Error, fmt, std::forward_like<Args>(args)...);
}

template<typename ...Args>
[[gnu::always_inline]] inline void Critical(std::format_string<Args...> fmt, Args&&... args)
{
    Logger::Internal::LogToStream(Logger::Critical, fmt, std::forward_like<Args>(args)...);
}

} // namespace Internal

} // namespace Logger

} // namespace Sage

// Log marcos for lazy va args evaluation

#define LOG_TRACE(fmt, ...) if (Sage::Logger::Internal::ShouldLog(Sage::Logger::Trace)) [[unlikely]] Sage::Logger::Internal::Trace(fmt, ## __VA_ARGS__)

#define LOG_DEBUG(fmt, ...) if (Sage::Logger::Internal::ShouldLog(Sage::Logger::Debug)) [[unlikely]] Sage::Logger::Internal::Debug(fmt, ## __VA_ARGS__)

#define LOG_INFO(fmt, ...) Sage::Logger::Internal::Info(fmt, ## __VA_ARGS__)

#define LOG_WARNING(fmt, ...) Sage::Logger::Internal::Warning(fmt, ## __VA_ARGS__)

#define LOG_ERROR(fmt, ...) Sage::Logger::Internal::Error(fmt, ## __VA_ARGS__)

#define LOG_CRITICAL(fmt, ...) Sage::Logger::Internal::Critical(fmt, ## __VA_ARGS__)
