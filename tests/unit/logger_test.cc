#include <gtest/gtest.h>
#include <fstream>
#include <filesystem>
#include <sstream>
#include "logger.h"
#include "config.h"

using namespace LogModule;

namespace {

class LoggerTest : public ::testing::Test {
protected:
    void SetUp() override {
        tempDir = std::filesystem::temp_directory_path() / "cpp_oj_logger_test";
        std::filesystem::create_directories(tempDir);
        logFile = tempDir / "test.log";

        std::string configContent = R"(
database:
  host: "localhost"
  port: 3306
  username: "test"
  password: ""
  name: "test"

server:
  host: "0.0.0.0"
  port: 8080

logging:
  level: "info"
  file: ")"
        + logFile.string() + R"(
)";
        auto configPath = tempDir / "config.yaml";
        std::ofstream(configPath) << configContent;
        oj::Config::getInstance().load(configPath.string());
    }

    void TearDown() override {
        std::filesystem::remove_all(tempDir);
    }

    std::filesystem::path tempDir;
    std::filesystem::path logFile;
};

TEST_F(LoggerTest, LogLevel2String) {
    EXPECT_EQ("DEBUG", LogLevel2String(LogLevel::DEBUG));
    EXPECT_EQ("INFO", LogLevel2String(LogLevel::INFO));
    EXPECT_EQ("WARNING", LogLevel2String(LogLevel::WARNING));
    EXPECT_EQ("ERROR", LogLevel2String(LogLevel::ERROR));
    EXPECT_EQ("FATAL", LogLevel2String(LogLevel::FATAL));
}

TEST_F(LoggerTest, GetTimeStampFormat) {
    std::string timestamp = LogModule::GetTimeStamp();
    EXPECT_EQ(19, timestamp.length());

    EXPECT_TRUE(timestamp[4] == '-');
    EXPECT_TRUE(timestamp[7] == '-');
    EXPECT_TRUE(timestamp[10] == ' ');
    EXPECT_TRUE(timestamp[13] == ':');
    EXPECT_TRUE(timestamp[16] == ':');
}

TEST_F(LoggerTest, LoggerSingleton) {
    LogModule::Logger &logger1 = LogModule::Logger::getInstance();
    LogModule::Logger &logger2 = LogModule::Logger::getInstance();
    EXPECT_EQ(&logger1, &logger2);
}

TEST_F(LoggerTest, ConsoleLogStrategy) {
    LogModule::Logger &logger = LogModule::Logger::getInstance();
    logger.UseConsoleLogStrategy();

    testing::internal::CaptureStdout();
    LOG(LogModule::LogLevel::INFO) << "Test message";
    std::string output = testing::internal::GetCapturedStdout();

    EXPECT_TRUE(output.find("[INFO]") != std::string::npos);
    EXPECT_TRUE(output.find("Test message") != std::string::npos);
}

TEST_F(LoggerTest, FileLogStrategy) {
    LogModule::Logger &logger = LogModule::Logger::getInstance();
    logger.UseFileLogStrategy(logFile.string());

    LOG(LogModule::LogLevel::INFO) << "File test message";

    ASSERT_TRUE(std::filesystem::exists(logFile));
    std::ifstream infile(logFile);
    std::stringstream buffer;
    buffer << infile.rdbuf();
    std::string content = buffer.str();

    EXPECT_TRUE(content.find("[INFO]") != std::string::npos);
    EXPECT_TRUE(content.find("File test message") != std::string::npos);
}

TEST_F(LoggerTest, MultipleLogLevels) {
    LogModule::Logger &logger = LogModule::Logger::getInstance();
    logger.UseFileLogStrategy(logFile.string());

    LOG(LogModule::LogLevel::DEBUG) << "Debug msg";
    LOG(LogModule::LogLevel::INFO) << "Info msg";
    LOG(LogModule::LogLevel::WARNING) << "Warning msg";
    LOG(LogModule::LogLevel::ERROR) << "Error msg";
    LOG(LogModule::LogLevel::FATAL) << "Fatal msg";

    std::ifstream infile(logFile);
    std::stringstream buffer;
    buffer << infile.rdbuf();
    std::string content = buffer.str();

    EXPECT_TRUE(content.find("[DEBUG]") != std::string::npos);
    EXPECT_TRUE(content.find("[WARNING]") != std::string::npos);
    EXPECT_TRUE(content.find("[ERROR]") != std::string::npos);
    EXPECT_TRUE(content.find("[FATAL]") != std::string::npos);
}

TEST_F(LoggerTest, LogMessageContainsTimestamp) {
    LogModule::Logger &logger = LogModule::Logger::getInstance();
    logger.UseFileLogStrategy(logFile.string());

    LOG(LogModule::LogLevel::INFO) << "Timestamp test";

    std::ifstream infile(logFile);
    std::stringstream buffer;
    buffer << infile.rdbuf();
    std::string content = buffer.str();

    EXPECT_TRUE(content.find("[INFO]") != std::string::npos);
}

TEST_F(LoggerTest, LogMessageContainsPid) {
    Logger &logger = Logger::getInstance();
    logger.UseFileLogStrategy(logFile.string());

    LOG(LogLevel::INFO) << "PID test";

    std::ifstream infile(logFile);
    std::stringstream buffer;
    buffer << infile.rdbuf();
    std::string content = buffer.str();

    EXPECT_TRUE(content.find("[") != std::string::npos);
    EXPECT_TRUE(content.find("]") != std::string::npos);
}

TEST_F(LoggerTest, MultipleMessagesAppended) {
    LogModule::Logger &logger = LogModule::Logger::getInstance();
    logger.UseFileLogStrategy(logFile.string());

    LOG(LogModule::LogLevel::INFO) << "First";
    LOG(LogModule::LogLevel::INFO) << "Second";
    LOG(LogModule::LogLevel::INFO) << "Third";

    std::ifstream infile(logFile);
    std::stringstream buffer;
    buffer << infile.rdbuf();
    std::string content = buffer.str();

    EXPECT_TRUE(content.find("First") != std::string::npos);
    EXPECT_TRUE(content.find("Second") != std::string::npos);
    EXPECT_TRUE(content.find("Third") != std::string::npos);
}

TEST_F(LoggerTest, InitLoggerWithEmptyFileFallsBackToConsole) {
    std::string configContent = R"(
database:
  host: "localhost"
  port: 3306
  username: "test"
  password: ""
  name: "test"

server:
  host: "0.0.0.0"
  port: 8080

logging:
  level: "info"
  file: ""
)";
    auto configPath = tempDir / "empty_config.yaml";
    std::ofstream(configPath) << configContent;
    oj::Config::getInstance().reset();
    oj::Config::getInstance().load(configPath.string());

    InitLogger();

    testing::internal::CaptureStdout();
    LOG(LogLevel::INFO) << "Console fallback test";
    std::string output = testing::internal::GetCapturedStdout();

    EXPECT_TRUE(output.find("Console fallback test") != std::string::npos);
}

TEST_F(LoggerTest, LogMessageWithVariousTypes) {
    LogModule::Logger &logger = LogModule::Logger::getInstance();
    logger.UseFileLogStrategy(logFile.string());

    LOG(LogModule::LogLevel::INFO) << "String" << 123 << 3.14 << true;

    std::ifstream infile(logFile);
    std::stringstream buffer;
    buffer << infile.rdbuf();
    std::string content = buffer.str();

    EXPECT_TRUE(content.find("String") != std::string::npos);
    EXPECT_TRUE(content.find("123") != std::string::npos);
    EXPECT_TRUE(content.find("3.14") != std::string::npos);
}

} // namespace