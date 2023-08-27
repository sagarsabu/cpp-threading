#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <mutex>
#include <sstream>
#include <iomanip>
#include <thread>
#include <unordered_map>

#include "log/logger.hpp"

namespace Sage::Log
{

// Aliases

using MsgBuffer = char[1024];
using MilliSecBuffer = char[16];
// Max allowed buffer for POSIX thread name
using ThreadNameBuffer = char[16];

// Formatter control

const char FORMAT_END[] = "\x1B[00m";
const char FORMAT_BOLD[] = "\x1B[01m";
const char FORMAT_DISABLED[] = "\x1B[02m";
const char FORMAT_ITALIC[] = "\x1B[03m";
const char FORMAT_URL[] = "\x1B[04m";
const char FORMAT_BLINK[] = "\x1B[05m";
const char FORMAT_BLINK2[] = "\x1B[06m";
const char FORMAT_SELECTED[] = "\x1B[07m";
const char FORMAT_INVISIBLE[] = "\x1B[08m";
const char FORMAT_STRIKE[] = "\x1B[09m";
const char FORMAT_DOUBLE_UNDERLINE[] = "\x1B[21m";

// Dark Colours

const char DARK_BLACK[] = "\x1B[30m";
const char DARK_RED[] = "\x1B[31m";
const char DARK_GREEN[] = "\x1B[32m";
const char DARK_YELLOW[] = "\x1B[33m";
const char DARK_BLUE[] = "\x1B[34m";
const char DARK_VIOLET[] = "\x1B[35m";
const char DARK_BEIGE[] = "\x1B[36m";
const char DARK_WHITE[] = "\x1B[37m";

// Light Colours

const char LIGHT_GREY[] = "\x1B[90m";
const char LIGHT_RED[] = "\x1B[91m";
const char LIGHT_GREEN[] = "\x1B[92m";
const char LIGHT_YELLOW[] = "\x1B[93m";
const char LIGHT_BLUE[] = "\x1B[94m";
const char LIGHT_VIOLET[] = "\x1B[95m";
const char LIGHT_BEIGE[] = "\x1B[96m";
const char LIGHT_WHITE[] = "\x1B[97m";

// Globals

const std::unordered_map<Level, const char*> g_levelColour
{
    {Level::Debug,      DARK_BLUE},
    {Level::Info,       DARK_WHITE},
    {Level::Warning,    LIGHT_YELLOW},
    {Level::Error,      LIGHT_RED},
    {Level::Critical,   DARK_RED},
};

const std::unordered_map<Level, const char*> g_levelInfo
{
    {Level::Debug,      "DEBUG"},
    {Level::Info,       "INFO "},
    {Level::Warning,    "WARN "},
    {Level::Error,      "ERROR"},
    {Level::Critical,   "CRIT "},
};

std::mutex g_logMutex;

Level g_currentLogLevel{ Level::Debug };

// Helper classes / structs

struct LogTimestamp
{
    LogTimestamp()
    {
        ::clock_gettime(CLOCK_REALTIME, &m_timeSpec);

        long millisec = m_timeSpec.tv_nsec / 1000000;
        // incase of overflow
        if (millisec >= 1000)
        {
            millisec -= 1000;
            m_timeSpec.tv_sec++;
        }

        snprintf(m_msSecBuff, sizeof(m_msSecBuff), ":%03ld", millisec);
    }

    inline const time_t& getSeconds() const { return m_timeSpec.tv_sec; }

    inline const time_t& getNanoSeconds() const { return m_timeSpec.tv_nsec; }

    inline const MilliSecBuffer& getMilliSecBuffer() const { return m_msSecBuff; };

private:
    MilliSecBuffer m_msSecBuff;
    ::timespec m_timeSpec;
};

// Functions

const char* getLevelFormatter(Level level) { return g_levelColour.at(level); }

const char* getLevelInfo(Level level) { return g_levelInfo.at(level); }

void SetLogLevel(Level logLevel) { g_currentLogLevel = logLevel; }

std::string GetThreadName()
{
    ThreadNameBuffer threadName;
    pthread_getname_np(pthread_self(), threadName, sizeof(threadName));

    std::ostringstream oss;
    oss << std::left << std::setw(sizeof(threadName)) << threadName;
    return oss.str();
}

void LogToStdOut(Level logLevel, const MsgBuffer& msgBuff)
{
    static const thread_local std::string threadName{ GetThreadName() };
    LogTimestamp ts;

    {
        std::scoped_lock lock{ g_logMutex };
        std::cout
            << getLevelFormatter(logLevel)
            << '[' << std::put_time(std::localtime(&ts.getSeconds()), "%d-%m-%Y %H:%M:%S") << ts.getMilliSecBuffer() << "] "
            << '[' << threadName << "] "
            << '[' << getLevelInfo(logLevel) << "] "
            << msgBuff
            << FORMAT_END << '\n';
    }
}

void Debug(const char* msg, ...)
{
    if (Level::Debug < g_currentLogLevel)
        return;

    MsgBuffer msgBuff;
    va_list args;
    va_start(args, msg);
    vsnprintf(msgBuff, sizeof(msgBuff), msg, args);
    va_end(args);

    LogToStdOut(Level::Debug, msgBuff);
}

void Info(const char* msg, ...)
{
    if (Level::Info < g_currentLogLevel)
        return;

    MsgBuffer msgBuff;
    va_list args;
    va_start(args, msg);
    vsnprintf(msgBuff, sizeof(msgBuff), msg, args);
    va_end(args);

    LogToStdOut(Level::Info, msgBuff);
}

void Warning(const char* msg, ...)
{
    if (Level::Warning < g_currentLogLevel)
        return;

    MsgBuffer msgBuff;
    va_list args;
    va_start(args, msg);
    vsnprintf(msgBuff, sizeof(msgBuff), msg, args);
    va_end(args);

    LogToStdOut(Level::Warning, msgBuff);
}

void Error(const char* msg, ...)
{
    if (Level::Error < g_currentLogLevel)
        return;

    MsgBuffer msgBuff;
    va_list args;
    va_start(args, msg);
    vsnprintf(msgBuff, sizeof(msgBuff), msg, args);
    va_end(args);

    LogToStdOut(Level::Error, msgBuff);
}

void Critical(const char* msg, ...)
{
    if (Level::Critical < g_currentLogLevel)
        return;

    MsgBuffer msgBuff;
    va_list args;
    va_start(args, msg);
    vsnprintf(msgBuff, sizeof(msgBuff), msg, args);
    va_end(args);

    LogToStdOut(Level::Critical, msgBuff);
}

} // namespace Sage::Log

