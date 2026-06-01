#include "logger.h"
#include "config.h"

namespace LogModule
{

std::string LogLevel2String(LogLevel level)
{
    switch (level)
    {
    case LogLevel::DEBUG:
        return "DEBUG";
    case LogLevel::INFO:
        return "INFO";
    case LogLevel::WARNING:
        return "WARNING";
    case LogLevel::ERROR:
        return "ERROR";
    case LogLevel::FATAL:
        return "FATAL";
    default:
        return "UNKNOWN";
    }
}

std::string GetTimeStamp()
{
    time_t currentTime = time(nullptr);
    struct tm dataTime;
    localtime_r(&currentTime, &dataTime);

    char dataTimeStr[128];
    snprintf(dataTimeStr, sizeof(dataTimeStr), "%4d-%02d-%02d %02d:%02d:%02d",
             dataTime.tm_year + 1900,
             dataTime.tm_mon + 1,
             dataTime.tm_mday,
             dataTime.tm_hour,
             dataTime.tm_min,
             dataTime.tm_sec);
    return dataTimeStr;
}

ConsoleLogStrategy::ConsoleLogStrategy() {}

ConsoleLogStrategy::~ConsoleLogStrategy() {}

void ConsoleLogStrategy::SyncLog(const std::string &message)
{
    std::lock_guard<std::mutex> lock(_mutex);
    std::cout << message << std::endl;
}

FileLogStrategy::FileLogStrategy(const std::string &logfilepath)
    : _logfilepath(logfilepath)
{
    std::lock_guard<std::mutex> lock(_mutex);
    std::ofstream out(_logfilepath, std::ios::app);
    (void)out;
}

FileLogStrategy::~FileLogStrategy() {}

void FileLogStrategy::SyncLog(const std::string &message)
{
    std::lock_guard<std::mutex> lock(_mutex);
    std::ofstream out(_logfilepath, std::ios::app);
    if (!out.is_open())
    {
        return;
    }
    out << message << "\n";
    out.close();
}

Logger &Logger::getInstance()
{
    static Logger instance;
    return instance;
}

void Logger::UseConsoleLogStrategy()
{
    _strategy = std::make_unique<ConsoleLogStrategy>();
}

void Logger::UseFileLogStrategy(const std::string &logfilepath)
{
    _strategy = std::make_unique<FileLogStrategy>(logfilepath);
}

Logger::LogMessage::LogMessage(LogLevel level, const std::string &filename, int line, Logger &self)
    : _currenttime(GetTimeStamp()),
      _loglevel(LogLevel2String(level)),
      _pid(getpid()),
      _filename(filename),
      _line(line),
      _logger(self)
{
    std::stringstream ss;
    ss << "[" << _currenttime << "] "
       << "[" << _loglevel << "] "
       << "[" << _pid << "] "
       << "[" << _filename << "] "
       << "[" << _line << "] "
       << "- ";
    _loginfo = ss.str();
}

Logger::LogMessage::~LogMessage()
{
    if (_logger._strategy)
    {
        _logger._strategy->SyncLog(_loginfo);
    }
}



Logger::LogMessage Logger::operator()(LogLevel level, const std::string &filename, int line)
{
    return LogMessage(level, filename, line, *this);
}

void InitLogger()
{
    auto &config = oj::Config::getInstance();
    std::string logFile = config.getLogFile();

    if (!logFile.empty())
    {
        logger.UseFileLogStrategy(logFile);
    }
    else
    {
        logger.UseConsoleLogStrategy();
    }
}

Logger &logger = Logger::getInstance();

}