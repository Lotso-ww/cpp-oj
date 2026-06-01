#ifndef OJ_LOGGER_H
#define OJ_LOGGER_H

#include <string>
#include <memory>
#include <fstream>
#include <iostream>
#include <sstream>
#include <ctime>
#include <unistd.h>
#include <mutex>

namespace LogModule
{

enum class LogLevel
{
    DEBUG,
    INFO,
    WARNING,
    ERROR,
    FATAL
};

std::string LogLevel2String(LogLevel level);
std::string GetTimeStamp();

class LogStrategy
{
public:
    virtual ~LogStrategy() = default;
    virtual void SyncLog(const std::string &message) = 0;
};

class ConsoleLogStrategy : public LogStrategy
{
public:
    ConsoleLogStrategy();
    ~ConsoleLogStrategy() override;
    void SyncLog(const std::string &message) override;

private:
    std::mutex _mutex;
};

class FileLogStrategy : public LogStrategy
{
public:
    explicit FileLogStrategy(const std::string &logfilepath);
    ~FileLogStrategy() override;
    void SyncLog(const std::string &message) override;

private:
    std::string _logfilepath;
    std::mutex _mutex;
};

class Logger
{
public:
    static Logger &getInstance();

    void UseConsoleLogStrategy();
    void UseFileLogStrategy(const std::string &logfilepath);

    class LogMessage
    {
    public:
        LogMessage(LogLevel level, const std::string &filename, int line, Logger &self);
        ~LogMessage();

        template <typename T>
        LogMessage &operator<<(const T &info)
        {
            std::stringstream ss;
            ss << info;
            _loginfo += ss.str();
            return *this;
        }

    private:
        std::string _currenttime;
        std::string _loglevel;
        int _pid;
        std::string _filename;
        int _line;
        std::string _loginfo;
        Logger &_logger;
    };

    LogMessage operator()(LogLevel level, const std::string &filename, int line);
    std::unique_ptr<LogStrategy> _strategy;
};

extern Logger &logger;

#define LOG(level) logger.getInstance()(level, __FILE__, __LINE__)

#define ENABLE_CONSOLE_LOG_STRATEGY() logger.UseConsoleLogStrategy()
#define ENABLE_FILE_LOG_STRATEGY(logfilepath) logger.UseFileLogStrategy(logfilepath)

void InitLogger();

}

#endif