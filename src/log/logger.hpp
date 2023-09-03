#pragma once

#include <optional>
#include <string>

namespace Sage
{

// So its accessible to the entire namespace
enum LogLevel
{
    Trace,
    Debug,
    Info,
    Warning,
    Error,
    Critical
};

template<LogLevel level>
[[gnu::format(printf, 1, 2)]]
void Log(const char* msg, ...);

namespace Logger
{

constexpr bool operator<(LogLevel lhs, LogLevel rhs)
{
    return (static_cast<uint8_t>(lhs) < static_cast<uint8_t>(rhs));
}

void SetLogLevel(LogLevel logLevel);

void SetupLogger(std::optional<std::string> filename = {});

} // namespace Logger


} // namespace Sage
