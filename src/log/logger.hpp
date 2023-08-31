#pragma once

#include <functional> // for std::hash

namespace Sage::Log
{

enum class Level
{
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

void SetupLogger(const std::string& optionalFilename = "");

void TeardownLogger();

} // namespace Sage::Log

// Template specialization for unordered_collections<Level>
template<>
struct std::hash<Sage::Log::Level>
{
    size_t operator()(Sage::Log::Level level) const noexcept
    {
        size_t hVal = std::hash<size_t>{}(static_cast<size_t>(level));
        return hVal;
    }
};
