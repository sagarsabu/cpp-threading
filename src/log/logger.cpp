#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <mutex>
#include <sstream>
#include <iomanip>
#include <thread>
#include <unordered_map>
#include <fstream>
#include <filesystem>
#include <atomic>

#include "log/logger.hpp"

namespace Sage::Log
{

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

std::recursive_mutex g_logMutex;

Level g_currentLogLevel{ Level::Debug };

struct LogStreamer;
std::atomic<LogStreamer*> g_logStreamer{ nullptr };

// Helper classes / structs

struct LogStreamer
{
    virtual ~LogStreamer() = default;

    virtual LogStreamer& operator <<(const char* log) = 0;
    virtual LogStreamer& operator <<(const char log) = 0;
};

struct CoutLogStreamer final : public LogStreamer
{
    virtual ~CoutLogStreamer() = default;

    CoutLogStreamer& operator<<(const char* log) override
    {
        std::cout << log;
        return *this;
    }

    CoutLogStreamer& operator<<(const char log) override
    {
        std::cout << log;
        return *this;
    }
};

struct FileLogStreamer final : public LogStreamer
{
    FileLogStreamer(const std::string& filename) :
        m_fileStream{ }
    {
        if (std::filesystem::is_regular_file(filename))
        {
            std::filesystem::permissions(filename, std::filesystem::perms::owner_write | std::filesystem::perms::group_read);
            m_fileStream = std::ofstream{ filename };
        }

        if (not m_fileStream.good())
        {
            throw std::runtime_error("Unable to open file '" + filename + "' for writing");
        }
    }

    virtual ~FileLogStreamer() = default;

    FileLogStreamer& operator<<(const char* log) override
    {
        std::cout << log;
        return *this;
    }

    FileLogStreamer& operator<<(const char log) override
    {
        std::cout << log;
        return *this;
    }

private:
    std::ofstream m_fileStream;
};

void SetupLogger(const std::string& optionalFilename)
{
    LogStreamer* logStreamer{ nullptr };
    try
    {
        if (not optionalFilename.empty())
        {
            FileLogStreamer* fileStreamer = new FileLogStreamer{ optionalFilename };
            logStreamer = fileStreamer;
        }
    }
    catch (const std::exception& e)
    {
        std::cerr << "FAILED to create file streamer. what: " << e.what() << '\n';
    }

    if (logStreamer == nullptr)
    {
        logStreamer = new CoutLogStreamer;
    }

    if (g_logStreamer != nullptr)
    {
        delete g_logStreamer;
    }

    g_logStreamer = logStreamer;
}

void TeardownLogger()
{
    if (g_logStreamer != nullptr)
    {
        delete g_logStreamer;
    }
}

struct LogTimestamp
{
    // e.g "01 - 09 - 2023 00:42 : 19"
    using SecondsBuffer = char[26];
    // :%03ld"
    using MilliSecBuffer = char[16];

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

        std::strftime(m_secondsBuffer, sizeof(m_secondsBuffer), "%d-%m-%Y %H:%M:%S", std::localtime(&m_timeSpec.tv_sec));
        snprintf(m_msSecBuff, sizeof(m_msSecBuff), ":%03ld", millisec);
    }

    inline const SecondsBuffer& getSecondsBuffer() const { return m_secondsBuffer; };

    inline const MilliSecBuffer& getMilliSecBuffer() const { return m_msSecBuff; };

private:
    SecondsBuffer m_secondsBuffer;
    MilliSecBuffer m_msSecBuff;
    ::timespec m_timeSpec;
};

// Functions

const char* getLevelFormatter(Level level) { return g_levelColour.at(level); }

const char* getLevelInfo(Level level) { return g_levelInfo.at(level); }

void SetLogLevel(Level logLevel) { g_currentLogLevel = logLevel; }

std::string GetThreadName()
{
    // Max allowed buffer for POSIX thread name
    using ThreadNameBuffer = char[16];

    ThreadNameBuffer threadName;
    pthread_getname_np(pthread_self(), threadName, sizeof(threadName));

    std::ostringstream oss;
    oss << std::left << std::setw(sizeof(threadName)) << threadName;
    return oss.str();
}

void LogToStreamer(Level logLevel, const char* msg, va_list args)
{
    using MsgBuffer = char[1024];

    static const thread_local std::string threadName{ GetThreadName() };

    MsgBuffer msgBuff;
    vsnprintf(msgBuff, sizeof(msgBuff), msg, args);

    LogTimestamp ts;
    const LogTimestamp::SecondsBuffer& secondsBuffer{ ts.getSecondsBuffer() };
    const LogTimestamp::MilliSecBuffer& milliSecBuffer{ ts.getMilliSecBuffer() };

    const char* levelFmt{ getLevelFormatter(logLevel) };
    const char* levelInfo{ getLevelInfo(logLevel) };

    {
        std::lock_guard lock{ g_logMutex };
        (*g_logStreamer)
            << levelFmt
            << '[' << secondsBuffer << milliSecBuffer << "] "
            << '[' << threadName.c_str() << "] "
            << '[' << levelInfo << "] "
            << msgBuff
            << FORMAT_END << '\n';
    }
}

void Debug(const char* msg, ...)
{
    if (Level::Debug < g_currentLogLevel)
        return;

    va_list args;
    va_start(args, msg);
    LogToStreamer(Level::Debug, msg, args);
    va_end(args);
}

void Info(const char* msg, ...)
{
    if (Level::Info < g_currentLogLevel)
        return;

    va_list args;
    va_start(args, msg);
    LogToStreamer(Level::Info, msg, args);
    va_end(args);
}

void Warning(const char* msg, ...)
{
    if (Level::Warning < g_currentLogLevel)
        return;

    va_list args;
    va_start(args, msg);
    LogToStreamer(Level::Warning, msg, args);
    va_end(args);
}

void Error(const char* msg, ...)
{
    if (Level::Error < g_currentLogLevel)
        return;

    va_list args;
    va_start(args, msg);
    LogToStreamer(Level::Error, msg, args);
    va_end(args);
}

void Critical(const char* msg, ...)
{
    if (Level::Critical < g_currentLogLevel)
        return;

    va_list args;
    va_start(args, msg);
    LogToStreamer(Level::Critical, msg, args);
    va_end(args);
}

} // namespace Sage::Log

