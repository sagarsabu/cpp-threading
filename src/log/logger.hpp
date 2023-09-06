#pragma once

#include <optional>
#include <string>

namespace Sage
{

namespace Log
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

} // namespace Log

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

constexpr bool operator<(Level lhs, Level rhs)
{
    return (static_cast<uint8_t>(lhs) < static_cast<uint8_t>(rhs));
}

void SetLogLevel(Level logLevel);

void SetupLogger(std::optional<std::string> filename = {});

} // namespace Logger

} // namespace Sage
