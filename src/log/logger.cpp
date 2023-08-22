#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <mutex>
#include <sstream>
#include <iomanip>

#include "log/logger.hpp"

namespace Sage::Log
{

constexpr size_t MAX_LOG_BUFF_SIZE = 1024;
constexpr size_t MILLI_SEC_BUFF_SIZE = 16;
std::mutex g_logMutex;

struct LogTimestamp
{
    char m_msSecBuff[MILLI_SEC_BUFF_SIZE];
    ::timespec m_ts;
};

LogTimestamp GetTimestamp()
{
    LogTimestamp timestamp;
    ::clock_gettime(CLOCK_REALTIME, &timestamp.m_ts);

    long millisec = timestamp.m_ts.tv_nsec / 1000000;
    // incase of overflow
    if (millisec >= 1000)
    {
        millisec -= 1000;
        timestamp.m_ts.tv_sec++;
    }

    snprintf(timestamp.m_msSecBuff, sizeof(timestamp.m_msSecBuff), ":%03ld", millisec);

    return timestamp;
}

void info(const char* msg, ...)
{
    char msgBuff[MAX_LOG_BUFF_SIZE];
    LogTimestamp timestamp{ GetTimestamp() };

    va_list args;
    va_start(args, msg);
    vsnprintf(msgBuff, sizeof(msgBuff), msg, args);
    va_end(args);

    {
        std::scoped_lock<std::mutex> logGuard{ g_logMutex };
        std::cout
            << '[' << std::put_time(std::localtime(&timestamp.m_ts.tv_sec), "%d-%m-%Y %H:%M:%S") << timestamp.m_msSecBuff << "] "
            << msgBuff << '\n';
    }
}

} // namespace Sage::Log

