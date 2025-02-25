#include <ctime>
#include <iostream>
#include <fstream>
#include <filesystem>

#include "log/logger.hpp"
#include "timers/timer.hpp"

namespace Sage
{

namespace Logger
{

void SetupLogger(const std::string& filename, Level logLevel)
{
    Internal::GetLogStreamer().Setup(filename, logLevel);
}

namespace Internal
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

// LogStreamer

void LogStreamer::Setup(const std::string& filename, Level level)
{
    m_logFilename = filename;
    m_logLevel = level;

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
        LOG_CRITICAL("==== failed to setup file logger. what: {} ====", e.what());
    }
}

void LogStreamer::EnsureLogFileWriteable()
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
            std::println(std::cerr, "cannot write to non regular file '{}'", m_logFilename);
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
            std::println(std::cerr, "unable to open file '{}' for writing", m_logFilename);
            return;
        }

        SetStreamToFile(std::move(file));

        LOG_CRITICAL("lost {} worth of logs", static_cast<decltype(s_logFileCreatorPeriod)>(m_lostLogTime));
        m_lostLogTime = {};
    }
    catch (const std::filesystem::filesystem_error& e)
    {
        std::println(std::cerr, "file system error when attempting to recreate file logger. e: {}", e.what());
    }
}

void LogStreamer::SetStreamToConsole()
{
    std::lock_guard lk{ m_mutex };
    m_logFileStream = {};
    m_streamRef = s_consoleStream;
}

void LogStreamer::SetStreamToFile(std::ofstream fileStream)
{
    std::lock_guard lk{ m_mutex };
    m_logFileStream = std::move(fileStream);
    m_streamRef = m_logFileStream;
}

// LogTimestamp

LogTimestamp::LogTimestamp() noexcept
{
    std::timespec_get(&m_timeSpec, TIME_UTC);
    std::tm localTimeRes{};
    std::strftime(
        m_secondsBuffer,
        sizeof(m_secondsBuffer),
        "%d-%m-%Y %H:%M:%S",
        ::localtime_r(&m_timeSpec.tv_sec, &localTimeRes)
    );
    snprintf(m_extraSecBuff, sizeof(m_extraSecBuff), ":%09lu", m_timeSpec.tv_nsec);
}


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

/**
 * Intentionally leaking here.
 * Logging in global object destructor's may cause issue if we destroy the logger
*/
LogStreamer* const g_logStreamer{ new LogStreamer };

// Functions

std::string_view GetLevelFormatter(Level level) noexcept { return LEVEL_COLOURS[level]; }

std::string_view GetLevelName(Level level) noexcept { return LEVEL_NAMES[level]; }

std::string_view GetFormatEnd() noexcept { return FORMAT_END; }

std::string_view CurrentThreadName() noexcept
{
    // Max allowed buffer for POSIX thread name
    using ThreadNameBuffer = char[16];

    static thread_local ThreadNameBuffer buff{};
    static thread_local bool init{ false };
    if (not init)
    {
        init = true;
        pthread_getname_np(pthread_self(), buff, sizeof(buff));
    }

    return { buff, sizeof(buff) };
}

LogStreamer& GetLogStreamer() noexcept { return *g_logStreamer; }

bool ShouldLog(Level level) noexcept { return level >= g_logStreamer->GetLogLevel(); }

} // namespace Internal

} // namespace Logger

} // namespace Sage
