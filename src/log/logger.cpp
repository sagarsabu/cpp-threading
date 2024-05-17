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

#include "log/logger.hpp"
#include "timers/timer.hpp"

namespace Sage
{

namespace Logger
{

// Formatter control

constexpr char FORMAT_END[] = "\x1B[00m";
constexpr char FORMAT_BOLD[] = "\x1B[01m";
constexpr char FORMAT_DISABLED[] = "\x1B[02m";
constexpr char FORMAT_ITALIC[] = "\x1B[03m";
constexpr char FORMAT_URL[] = "\x1B[04m";
constexpr char FORMAT_BLINK[] = "\x1B[05m";
constexpr char FORMAT_BLINK2[] = "\x1B[06m";
constexpr char FORMAT_SELECTED[] = "\x1B[07m";
constexpr char FORMAT_INVISIBLE[] = "\x1B[08m";
constexpr char FORMAT_STRIKE[] = "\x1B[09m";
constexpr char FORMAT_DOUBLE_UNDERLINE[] = "\x1B[21m";

// Dark Colours

constexpr char DARK_BLACK[] = "\x1B[30m";
constexpr char DARK_RED[] = "\x1B[31m";
constexpr char DARK_GREEN[] = "\x1B[32m";
constexpr char DARK_YELLOW[] = "\x1B[33m";
constexpr char DARK_BLUE[] = "\x1B[34m";
constexpr char DARK_VIOLET[] = "\x1B[35m";
constexpr char DARK_BEIGE[] = "\x1B[36m";
constexpr char DARK_WHITE[] = "\x1B[37m";

// Light Colours

constexpr char LIGHT_GREY[] = "\x1B[90m";
constexpr char LIGHT_RED[] = "\x1B[91m";
constexpr char LIGHT_GREEN[] = "\x1B[92m";
constexpr char LIGHT_YELLOW[] = "\x1B[93m";
constexpr char LIGHT_BLUE[] = "\x1B[94m";
constexpr char LIGHT_VIOLET[] = "\x1B[95m";
constexpr char LIGHT_BEIGE[] = "\x1B[96m";
constexpr char LIGHT_WHITE[] = "\x1B[97m";

// Helper classes / structs

class LogStreamer
{
public:
    using Stream = std::basic_ostream<char>;

    LogStreamer() = default;

    void Setup(const std::string& filename)
    {
        m_logFilename = filename;

        if (m_logFilename.empty())
        {
            SetStreamToConsole();
            return;
        }

        // try setup a file logger if specified
        try
        {
            if (std::filesystem::exists(m_logFilename) and not std::filesystem::is_regular_file(m_logFilename))
            {
                throw std::runtime_error("cannot write to non regular file '" + m_logFilename + "'");
            }

            std::ofstream file{ m_logFilename , std::ios::out | std::ios::ate | std::ios::app };
            std::filesystem::permissions(
                m_logFilename,
                std::filesystem::perms::owner_write | std::filesystem::perms::group_read,
                std::filesystem::perm_options::add
            );

            if (file.fail())
            {
                throw std::runtime_error("unable to open file '" + m_logFilename + "' for writing");
            }

            SetStreamToFile(std::move(file));

            m_logFileCreator = std::make_unique<PeriodicTimer>(s_logFileCreatorPeriod, [this] { EnsureLogFileWriteable(); });
            m_logFileCreator->Start();
        }
        catch (const std::exception& e)
        {
            Log::Critical("==== failed to setup file logger. what: %s ====", e.what());
        }
    }

private:
    // Nothing in here is movable or copyable
    LogStreamer(const LogStreamer&) = delete;
    LogStreamer(LogStreamer&&) = delete;
    LogStreamer& operator=(const LogStreamer&) = delete;
    LogStreamer& operator=(LogStreamer&&) = delete;

    void EnsureLogFileWriteable()
    {
        if (m_logFilename.empty())
        {
            return;
        }

        try
        {
            // everything is okay
            if (std::filesystem::exists(m_logFilename) and std::filesystem::is_regular_file(m_logFilename))
            {
                return;
            }

            // No longer writing to log file

            m_lostLogTime += s_logFileCreatorPeriod.count();

            if (std::filesystem::exists(m_logFilename) and not std::filesystem::is_regular_file(m_logFilename))
            {
                std::fprintf(::stderr, "cannot write to non regular file '%s'", m_logFilename.c_str());
                return;
            }

            std::ofstream file{ m_logFilename , std::ios::out | std::ios::ate | std::ios::app };
            std::filesystem::permissions(
                m_logFilename,
                std::filesystem::perms::owner_write | std::filesystem::perms::group_read,
                std::filesystem::perm_options::add
            );

            if (file.fail())
            {
                std::fprintf(::stderr, "unable to open file '%s' for writing", m_logFilename.c_str());
                return;
            }

            SetStreamToFile(std::move(file));

            std::ostringstream lostLogTimeOSS;
            lostLogTimeOSS << static_cast<decltype(s_logFileCreatorPeriod)>(m_lostLogTime);
            Log::Critical("lost %s worth of logs", lostLogTimeOSS.str().c_str());
            m_lostLogTime = {};
        }
        catch (const std::filesystem::filesystem_error& e)
        {
            std::fprintf(::stderr, "file system error when attempting to recreate file logger. e: %s", e.what());
        }
    }

    void SetStreamToConsole()
    {
        std::lock_guard lk{ m_mutex };
        m_logFileStream = {};
        m_streamRef = s_consoleStream;
    }

    void SetStreamToFile(std::ofstream fileStream)
    {
        std::lock_guard lk{ m_mutex };
        m_logFileStream = std::move(fileStream);
        m_streamRef = m_logFileStream;
    }

private:
    static constexpr TimeS s_logFileCreatorPeriod{ 60s };
    static constexpr std::reference_wrapper<Stream> s_consoleStream{ std::clog };

    std::reference_wrapper<Stream> m_streamRef{ s_consoleStream };
    std::recursive_mutex m_mutex{};
    std::string m_logFilename{};
    size_t m_lostLogTime{ 0 };
    std::ofstream m_logFileStream{};
    std::unique_ptr<PeriodicTimer> m_logFileCreator{ nullptr };

    friend void LogToStream(Level level, const char* fmt, va_list args);
};

struct LogTimestamp
{
    // e.g "01 - 09 - 2023 00:42 : 19"
    using SecondsBuffer = char[26];
    // :%03u requires 7 bytes max
    using MilliSecBuffer = char[7];

    LogTimestamp() noexcept
    {
        std::timespec_get(&m_timeSpec, TIME_UTC);

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

    inline const SecondsBuffer& getSecondsBuffer() const noexcept { return m_secondsBuffer; };

    inline const MilliSecBuffer& getMilliSecBuffer() const noexcept { return m_msSecBuff; };

private:
    SecondsBuffer m_secondsBuffer;
    MilliSecBuffer m_msSecBuff;
    ::timespec m_timeSpec;
};

// For std::unordered_map<Logger::Level, ...>
constexpr bool operator<(Level lhs, Level rhs) noexcept { return (static_cast<uint8_t>(lhs) < static_cast<uint8_t>(rhs)); }

// Global variables

const std::unordered_map<Level, const char*> g_levelColour
{
    {Level::Trace,      LIGHT_GREEN},
    {Level::Debug,      DARK_BLUE},
    {Level::Info,       DARK_WHITE},
    {Level::Warning,    LIGHT_YELLOW},
    {Level::Error,      LIGHT_RED},
    {Level::Critical,   DARK_RED},
};

const std::unordered_map<Level, const char*> g_levelInfo
{
    {Level::Trace,      "TRACE"},
    {Level::Debug,      "DEBUG"},
    {Level::Info,       "INFO "},
    {Level::Warning,    "WARN "},
    {Level::Error,      "ERROR"},
    {Level::Critical,   "CRIT "},
};

Level g_currentLogLevel{ Level::Debug };

LogStreamer g_logStreamer;

const thread_local std::string g_threadName{ LogFriendlyGetThreadName() };

// Functions

void SetLogLevel(Level logLevel) { g_currentLogLevel = logLevel; }

void SetupLogger(const std::string& filename)
{
    g_logStreamer.Setup(filename);
}

inline const char* GetLevelFormatter(Level level) { return g_levelColour.at(level); }

inline const char* GetLevelInfo(Level level) { return g_levelInfo.at(level); }

std::string LogFriendlyGetThreadName()
{
    // Max allowed buffer for POSIX thread name
    using ThreadNameBuffer = char[16];

    ThreadNameBuffer threadName;
    pthread_getname_np(pthread_self(), threadName, sizeof(threadName));

    std::ostringstream oss;
    oss << std::left << std::setw(sizeof(threadName)) << threadName;
    return oss.str();
}

void LogToStream(Level level, const char* fmt, va_list args)
{
    using MsgBuffer = char[1024];

    MsgBuffer msgBuff;
    vsnprintf(msgBuff, sizeof(msgBuff), fmt, args);

    LogTimestamp ts;
    const LogTimestamp::SecondsBuffer& secondsBuffer{ ts.getSecondsBuffer() };
    const LogTimestamp::MilliSecBuffer& milliSecBuffer{ ts.getMilliSecBuffer() };

    const char* levelFmt{ GetLevelFormatter(level) };
    const char* levelInfo{ GetLevelInfo(level) };

    {
        std::lock_guard lock{ g_logStreamer.m_mutex };
        LogStreamer::Stream& stream{ g_logStreamer.m_streamRef.get() };
        stream
            << levelFmt
            << '[' << secondsBuffer << milliSecBuffer << "] "
            << '[' << g_threadName << "] "
            << '[' << levelInfo << "] "
            << msgBuff
            << FORMAT_END << '\n';
        std::flush(stream);
    }
}

} // namespace Logger

namespace Log
{

void Trace(const char* msg, ...)
{
    if (Logger::Trace < Logger::g_currentLogLevel)
        return;

    va_list args;
    va_start(args, msg);
    Logger::LogToStream(Logger::Trace, msg, args);
    va_end(args);
}

void Debug(const char* msg, ...)
{
    if (Logger::Debug < Logger::g_currentLogLevel)
        return;

    va_list args;
    va_start(args, msg);
    Logger::LogToStream(Logger::Debug, msg, args);
    va_end(args);
}

void Info(const char* msg, ...)
{
    if (Logger::Info < Logger::g_currentLogLevel)
        return;

    va_list args;
    va_start(args, msg);
    Logger::LogToStream(Logger::Info, msg, args);
    va_end(args);
}

void Warning(const char* msg, ...)
{
    if (Logger::Warning < Logger::g_currentLogLevel)
        return;

    va_list args;
    va_start(args, msg);
    Logger::LogToStream(Logger::Warning, msg, args);
    va_end(args);
}

void Error(const char* msg, ...)
{
    if (Logger::Error < Logger::g_currentLogLevel)
        return;

    va_list args;
    va_start(args, msg);
    Logger::LogToStream(Logger::Error, msg, args);
    va_end(args);
}

void Critical(const char* msg, ...)
{
    if (Logger::Critical < Logger::g_currentLogLevel)
        return;

    va_list args;
    va_start(args, msg);
    Logger::LogToStream(Logger::Critical, msg, args);
    va_end(args);
}

} // namespace Log

} // namespace Sage
