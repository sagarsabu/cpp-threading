#pragma once

#include <string>
#include <format>
#include <print>

#include "timers/timer.hpp"
#include "log/log_stream.hpp"

namespace Sage
{

namespace Logger
{

void SetupLogger(const std::string& filename = "", Level logLevel = Level::Info);

namespace Internal
{

std::string_view GetLevelFormatter(Level level) noexcept;

std::string_view GetLevelName(Level level) noexcept;

std::string_view GetFormatEnd() noexcept;

struct LogTimestamp
{
    // e.g "01 - 09 - 2023 00:42 : 19"
    using SecondsBuffer = char[26];
    // :%09lu requires 22 bytes max
    using NanoSecBuffer = char[22];

    SecondsBuffer m_s;
    NanoSecBuffer m_ns;
};

LogTimestamp GetCurrentTimeStamp() noexcept;

std::string_view CurrentThreadName() noexcept;

inline bool ShouldLog(Level level) noexcept { return level >= GetLogStreamer().GetLogLevel(); }

template<typename ...Args>
 inline void LogToStream(Level level, std::format_string<Args...> fmt, Args&&... args)
{
    if (not Internal::ShouldLog(level))
        return;

    LogTimestamp ts{ GetCurrentTimeStamp() };

    {
        auto& logStreamer{ GetLogStreamer() };
        std::lock_guard lock{ logStreamer.m_mutex };
        LogStreamer::Stream& stream{ logStreamer.m_streamRef.get() };
        std::println(
            stream,
            "{}[{}{}] [{}] [{}] {}{}",
            GetLevelFormatter(level),
            ts.m_s, ts.m_ns,
            CurrentThreadName(),
            GetLevelName(level),
            std::format(fmt, std::forward_like<Args>(args)...),
            GetFormatEnd()
        );
        std::flush(stream);
    }
}

template<typename ...Args>
 inline void Trace(std::format_string<Args...> fmt, Args&&... args)
{
    Logger::Internal::LogToStream(Logger::Trace, fmt, std::forward_like<Args>(args)...);
}

template<typename ...Args>
 inline void Debug(std::format_string<Args...> fmt, Args&&... args)
{
    Logger::Internal::LogToStream(Logger::Debug, fmt, std::forward_like<Args>(args)...);
}

template<typename ...Args>
 inline void Info(std::format_string<Args...> fmt, Args&&... args)
{
    Logger::Internal::LogToStream(Logger::Info, fmt, std::forward_like<Args>(args)...);
}

template<typename ...Args>
 inline void Warning(std::format_string<Args...> fmt, Args&&... args)
{
    Logger::Internal::LogToStream(Logger::Warning, fmt, std::forward_like<Args>(args)...);
}

template<typename ...Args>
 inline void Error(std::format_string<Args...> fmt, Args&&... args)
{
    Logger::Internal::LogToStream(Logger::Error, fmt, std::forward_like<Args>(args)...);
}

template<typename ...Args>
 inline void Critical(std::format_string<Args...> fmt, Args&&... args)
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
