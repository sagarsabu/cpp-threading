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

namespace Sage
{

namespace Logger
{

// Helper classes / structs

// TODO: Find a way to do this by avoiding vtables

class LogStreamer
{
public:
    virtual ~LogStreamer() = default;

    virtual inline LogStreamer& operator <<(const char* log) = 0;
    virtual inline LogStreamer& operator <<(const char log) = 0;
    virtual inline void flush() = 0;
};

class CoutLogStreamer final : public LogStreamer
{
public:
    virtual ~CoutLogStreamer() = default;

    inline CoutLogStreamer& operator<<(const char* log) override
    {
        std::cout << log;
        return *this;
    }

    inline CoutLogStreamer& operator<<(const char log) override
    {
        std::cout << log;
        return *this;
    }

    inline void flush() override
    {
        // Don't actually need to flush stdout
    }
};

class FileLogStreamer final : public LogStreamer
{
public:
    FileLogStreamer() :
        m_fileStream{ }
    { }

    virtual ~FileLogStreamer() = default;

    /**
     @throws std::exception on failure
    */
    void SetLogFile(const std::string& filename)
    {
        if (std::filesystem::exists(filename) and not std::filesystem::is_regular_file(filename))
        {
            throw std::runtime_error("Cannot write to non regular file '" + filename + "'");
        }

        std::ofstream file{ filename , std::ios::out | std::ios::ate | std::ios::app };
        std::filesystem::permissions(filename,
            std::filesystem::perms::owner_write | std::filesystem::perms::group_read,
            std::filesystem::perm_options::add
        );

        if (not file.is_open())
        {
            throw std::runtime_error("Unable to open file '" + filename + "' for writing");
        }

        m_fileStream = std::move(file);
    }

    inline FileLogStreamer& operator<<(const char* log) override
    {
        m_fileStream << log;
        return *this;
    }

    inline FileLogStreamer& operator<<(const char log) override
    {
        m_fileStream << log;
        return *this;
    }

    inline void flush() override
    {
        std::flush(m_fileStream);
    }

private:
    std::ofstream m_fileStream;
};

// All global variables visible only to this translation unit

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

const std::unordered_map<LogLevel, const char*> g_levelColour
{
    {LogLevel::Trace,      LIGHT_GREEN},
    {LogLevel::Debug,      DARK_BLUE},
    {LogLevel::Info,       DARK_WHITE},
    {LogLevel::Warning,    LIGHT_YELLOW},
    {LogLevel::Error,      LIGHT_RED},
    {LogLevel::Critical,   DARK_RED},
};

const std::unordered_map<LogLevel, const char*> g_levelInfo
{
    {LogLevel::Trace,      "TRACE"},
    {LogLevel::Debug,      "DEBUG"},
    {LogLevel::Info,       "INFO "},
    {LogLevel::Warning,    "WARN "},
    {LogLevel::Error,      "ERROR"},
    {LogLevel::Critical,   "CRIT "},
};

std::recursive_mutex g_logMutex;

LogLevel g_currentLogLevel{ LogLevel::Debug };

FileLogStreamer g_fileStreamer;

CoutLogStreamer g_coutLogStreamer;

// Default to cout streamer
std::atomic<LogStreamer*> g_logStreamer{ &g_coutLogStreamer };

void SetupLogger(std::optional<std::string> filename)
{
    LogStreamer* logStreamer{ nullptr };

    // try setup a file logger is specified
    if (filename)
    {
        try
        {
            g_fileStreamer.SetLogFile(*filename);
            logStreamer = &g_fileStreamer;
        }
        catch (const std::exception& e)
        {
            std::cerr << "FAILED to create file streamer. what: " << e.what() << std::endl;
        }
    }

    // Default to stdout
    if (logStreamer == nullptr)
    {
        logStreamer = &g_coutLogStreamer;
    }

    g_logStreamer = logStreamer;
}

struct LogTimestamp
{
    // e.g "01 - 09 - 2023 00:42 : 19"
    using SecondsBuffer = char[26];
    // :%03u requires 7 bytes max
    using MilliSecBuffer = char[7];

    LogTimestamp()
    {
        ::clock_gettime(CLOCK_REALTIME, &m_timeSpec);

        uint16_t millisec = static_cast<uint16_t>(m_timeSpec.tv_nsec / 1'000'000U);
        // incase of overflow
        if (millisec >= 1000U)
        {
            millisec = static_cast<uint16_t>(millisec - 1000U);
            m_timeSpec.tv_sec++;
        }

        std::strftime(m_secondsBuffer, sizeof(m_secondsBuffer), "%d-%m-%Y %H:%M:%S", std::localtime(&m_timeSpec.tv_sec));
        snprintf(m_msSecBuff, sizeof(m_msSecBuff), ":%03u", millisec);
    }

    inline const SecondsBuffer& getSecondsBuffer() const { return m_secondsBuffer; };

    inline const MilliSecBuffer& getMilliSecBuffer() const { return m_msSecBuff; };

private:
    SecondsBuffer m_secondsBuffer;
    MilliSecBuffer m_msSecBuff;
    ::timespec m_timeSpec;
};

// Functions

const char* getLevelFormatter(LogLevel level) { return g_levelColour.at(level); }

const char* getLevelInfo(LogLevel level) { return g_levelInfo.at(level); }

void SetLogLevel(LogLevel logLevel) { g_currentLogLevel = logLevel; }

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

void LogToStreamer(LogLevel logLevel, const char* msg, va_list args)
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
        (*g_logStreamer).flush();
    }
}

} // namespace Logger

template<LogLevel level>
void Log(const char* msg, ...)
{
    if (level < Logger::g_currentLogLevel)
        return;

    va_list args;
    va_start(args, msg);
    Logger::LogToStreamer(level, msg, args);
    va_end(args);
}

// Initialize all the logging functions we want

template void Log<LogLevel::Trace>(const char* msg, ...);

template void Log<LogLevel::Debug>(const char* msg, ...);

template void Log<LogLevel::Info>(const char* msg, ...);

template void Log<LogLevel::Warning>(const char* msg, ...);

template void Log<LogLevel::Error>(const char* msg, ...);

template void Log<LogLevel::Critical>(const char* msg, ...);


} // namespace Sage
