#include <cstdarg>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <mutex>
#include <thread>
#include <fstream>
#include <filesystem>
#include <format>

#include "log/logger.hpp"
#include "timers/timer.hpp"

namespace Sage
{

namespace Logger
{

// Formatter control

constexpr std::string_view FORMAT_END{ "\x1B[00m" };
constexpr std::string_view FORMAT_BOLD{ "\x1B[01m" };
constexpr std::string_view FORMAT_DISABLED{ "\x1B[02m" };
constexpr std::string_view FORMAT_ITALIC{ "\x1B[03m" };
constexpr std::string_view FORMAT_URL{ "\x1B[04m" };
constexpr std::string_view FORMAT_BLINK{ "\x1B[05m" };
constexpr std::string_view FORMAT_BLINK2{ "\x1B[06m" };
constexpr std::string_view FORMAT_SELECTED{ "\x1B[07m" };
constexpr std::string_view FORMAT_INVISIBLE{ "\x1B[08m" };
constexpr std::string_view FORMAT_STRIKE{ "\x1B[09m" };
constexpr std::string_view FORMAT_DOUBLE_UNDERLINE{ "\x1B[21m" };

// Dark Colours

constexpr std::string_view DARK_BLACK{ "\x1B[30m" };
constexpr std::string_view DARK_RED{ "\x1B[31m" };
constexpr std::string_view DARK_GREEN{ "\x1B[32m" };
constexpr std::string_view DARK_YELLOW{ "\x1B[33m" };
constexpr std::string_view DARK_BLUE{ "\x1B[34m" };
constexpr std::string_view DARK_VIOLET{ "\x1B[35m" };
constexpr std::string_view DARK_BEIGE{ "\x1B[36m" };
constexpr std::string_view DARK_WHITE{ "\x1B[37m" };

// Light Colours

constexpr std::string_view LIGHT_GREY{ "\x1B[90m" };
constexpr std::string_view LIGHT_RED{ "\x1B[91m" };
constexpr std::string_view LIGHT_GREEN{ "\x1B[92m" };
constexpr std::string_view LIGHT_YELLOW{ "\x1B[93m" };
constexpr std::string_view LIGHT_BLUE{ "\x1B[94m" };
constexpr std::string_view LIGHT_VIOLET{ "\x1B[95m" };
constexpr std::string_view LIGHT_BEIGE{ "\x1B[96m" };
constexpr std::string_view LIGHT_WHITE{ "\x1B[97m" };

// Helper classes / structs

class LogStreamer
{
public:
    using Stream = std::ostream;

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

            m_logFileCreator = std::make_unique<PeriodicTimer>("LogFileCreator", s_logFileCreatorPeriod, [this] { EnsureLogFileWriteable(); });
            m_logFileCreator->Start();
        }
        catch (const std::exception& e)
        {
            LOG_CRITICAL("==== failed to setup file logger. what: %s ====", e.what());
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
            LOG_CRITICAL("lost %s worth of logs", lostLogTimeOSS.str().c_str());
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

        std::tm localTimeRes{};
        std::strftime(
            m_secondsBuffer,
            sizeof(m_secondsBuffer),
            "%d-%m-%Y %H:%M:%S",
            ::localtime_r(&m_timeSpec.tv_sec, &localTimeRes)
        );
        snprintf(m_msSecBuff, sizeof(m_msSecBuff), ":%03u", millisec);
    }

    constexpr const SecondsBuffer& getSecondsBuffer() const noexcept { return m_secondsBuffer; };

    constexpr const MilliSecBuffer& getMilliSecBuffer() const noexcept { return m_msSecBuff; };

private:
    SecondsBuffer m_secondsBuffer;
    MilliSecBuffer m_msSecBuff;
    ::timespec m_timeSpec;
};

// Global variables

constexpr std::array<std::string_view, Level::Critical + 1> LEVEL_COLOURS
{
    LIGHT_GREEN,    // Level::Trace
    DARK_BLUE,      // Level::Debug
    DARK_WHITE,     // Level::Info
    LIGHT_YELLOW,   // Level::Warning
    LIGHT_RED,      // Level::Error
    DARK_RED,       // Level::Critical
};

constexpr std::array<std::string_view, Level::Critical + 1> LEVEL_NAMES
{
    "TRACE",        // Level::Trace
    "DEBUG",        // Level::Debug
    "INFO ",        // Level::Info
    "WARN ",        // Level::Warning
    "ERROR",        // Level::Error
    "CRIT ",        // Level::Critical
};

Level g_currentLogLevel{ Level::Info };

/**
 * Intentionally leaking here.
 * Logging in global object destructor's may cause issue if we destroy the logger
*/
LogStreamer* const g_logStreamer{ new LogStreamer };

const thread_local std::string g_threadName{ Internal::LogFriendlyThreadName() };

// Functions

void SetLogLevel(Level logLevel) { g_currentLogLevel = logLevel; }

void SetupLogger(const std::string& filename)
{
    g_logStreamer->Setup(filename);
}

constexpr std::string_view GetLevelFormatter(Level level) noexcept { return LEVEL_COLOURS[level]; }

constexpr std::string_view GetLevelName(Level level) noexcept { return LEVEL_NAMES[level]; }

void LogToStream(Level level, const char* fmt, va_list args)
{
    using MsgBuffer = char[1024];

    LogTimestamp ts;
    MsgBuffer msgBuff;
    vsnprintf(msgBuff, sizeof(msgBuff), fmt, args);

    {
        std::lock_guard lock{ g_logStreamer->m_mutex };
        LogStreamer::Stream& stream{ g_logStreamer->m_streamRef.get() };
        stream
            << GetLevelFormatter(level)
            << '[' << ts.getSecondsBuffer() << ts.getMilliSecBuffer() << "] "
            << '[' << g_threadName << "] "
            << '[' << GetLevelName(level) << "] "
            << msgBuff
            << FORMAT_END << '\n';
        std::flush(stream);
    }
}

namespace Internal
{

void Trace(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    Logger::LogToStream(Logger::Trace, fmt, args);
    va_end(args);
}

void Debug(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    Logger::LogToStream(Logger::Debug, fmt, args);
    va_end(args);
}

void Info(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    Logger::LogToStream(Logger::Info, fmt, args);
    va_end(args);
}

void Warning(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    Logger::LogToStream(Logger::Warning, fmt, args);
    va_end(args);
}

void Error(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    Logger::LogToStream(Logger::Error, fmt, args);
    va_end(args);
}

void Critical(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    Logger::LogToStream(Logger::Critical, fmt, args);
    va_end(args);
}

std::string LogFriendlyThreadName()
{
    // Max allowed buffer for POSIX thread name
    using ThreadNameBuffer = char[16];

    ThreadNameBuffer threadName;
    pthread_getname_np(pthread_self(), threadName, sizeof(threadName));

    // centered thread name output
    const std::string fmt{ "{:^" + std::to_string(sizeof(threadName)) + "s}" };
    return std::vformat(fmt, std::make_format_args(threadName));
}

bool ShouldLog(Level level) noexcept { return level >= g_currentLogLevel; }

} // namespace Internal

} // namespace Logger

} // namespace Sage
