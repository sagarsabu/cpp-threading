#pragma once

#include <string>

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

void SetLogLevel(Level logLevel);

void SetupLogger(const std::string& filename = "");

namespace Internal
{

[[gnu::format(printf, 1, 2)]]
void Trace(const char* msg, ...);

[[gnu::format(printf, 1, 2)]]
void Debug(const char* msg, ...);

[[gnu::format(printf, 1, 2)]]
void Info(const char* msg, ...);

[[gnu::format(printf, 1, 2)]]
void Warning(const char* msg, ...);

[[gnu::format(printf, 1, 2)]]
void Error(const char* msg, ...);

[[gnu::format(printf, 1, 2)]]
void Critical(const char* msg, ...);

std::string LogFriendlyGetThreadName();

bool ShouldLog(Level level) noexcept;

} // namespace Internal

} // namespace Logger

} // namespace Sage

// Log marcos for lazy va args evaluation

#define LOG_TRACE(fmt, ...) if (Sage::Logger::Internal::ShouldLog(Sage::Logger::Trace)) Sage::Logger::Internal::Trace((fmt), ## __VA_ARGS__)

#define LOG_DEBUG(fmt, ...) if (Sage::Logger::Internal::ShouldLog(Sage::Logger::Debug)) Sage::Logger::Internal::Debug((fmt), ## __VA_ARGS__)

#define LOG_INFO(fmt, ...) if (Sage::Logger::Internal::ShouldLog(Sage::Logger::Info)) Sage::Logger::Internal::Info((fmt), ## __VA_ARGS__)

#define LOG_WARNING(fmt, ...) if (Sage::Logger::Internal::ShouldLog(Sage::Logger::Warning)) Sage::Logger::Internal::Warning((fmt), ## __VA_ARGS__)

#define LOG_ERROR(fmt, ...) if (Sage::Logger::Internal::ShouldLog(Sage::Logger::Error)) Sage::Logger::Internal::Error((fmt), ## __VA_ARGS__)

#define LOG_CRITICAL(fmt, ...) if (Sage::Logger::Internal::ShouldLog(Sage::Logger::Critical)) Sage::Logger::Internal::Critical((fmt), ## __VA_ARGS__)
