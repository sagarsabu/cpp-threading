#pragma once

#include <optional>
#include <string>
#include <cstdint>

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

void SetLogLevel(Level logLevel);

void SetupLogger(const std::string& filename = "");

std::string LogFriendlyGetThreadName();

} // namespace Logger

} // namespace Sage
