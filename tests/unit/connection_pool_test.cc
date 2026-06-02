#include <gtest/gtest.h>
#include <fstream>
#include <filesystem>
#include <thread>
#include <atomic>
#include "connection_pool.h"
#include "config.h"
#include "logger.h"

using namespace LogModule;

namespace {

class ConnectionPoolTest : public ::testing::Test {
protected:
    void SetUp() override {
        tempDir = std::filesystem::temp_directory_path() / "cpp_oj_connection_pool_test";
        std::filesystem::create_directories(tempDir);
        configFile = tempDir / "config.yaml";

        std::string configContent = R"(
database:
  host: "localhost"
  port: 3306
  username: "lotso"
  password: ""
  name: "oj_system"

server:
  host: "0.0.0.0"
  port: 8080

connection_pool:
  size: 5

timeouts:
  request: 5000
  compile: 10000
  run: 5000

logging:
  level: "debug"
  file: ""
)";
        std::ofstream(configFile) << configContent;
        oj::Config::getInstance().load(configFile.string());
        LogModule::ENABLE_CONSOLE_LOG_STRATEGY();
    }

    void TearDown() override {
        std::filesystem::remove_all(tempDir);
    }

    std::filesystem::path tempDir;
    std::filesystem::path configFile;
};

TEST_F(ConnectionPoolTest, SingletonPattern) {
    auto& pool1 = oj::ConnectionPool::getInstance();
    auto& pool2 = oj::ConnectionPool::getInstance();
    EXPECT_EQ(&pool1, &pool2);
}

TEST_F(ConnectionPoolTest, PoolSizeFromConfig) {
    auto& pool = oj::ConnectionPool::getInstance();
    EXPECT_EQ(pool.getPoolSize(), 5);
}

TEST_F(ConnectionPoolTest, InitialAvailableCount) {
    auto& pool = oj::ConnectionPool::getInstance();
    EXPECT_EQ(pool.getAvailableCount(), 5);
}

TEST_F(ConnectionPoolTest, GetAndReleaseConnection) {
    auto conn1 = oj::getConnection();
    ASSERT_NE(conn1, nullptr);

    EXPECT_EQ(oj::ConnectionPool::getInstance().getAvailableCount(), 4);

    oj::releaseConnection(conn1);

    EXPECT_EQ(oj::ConnectionPool::getInstance().getAvailableCount(), 5);
}

TEST_F(ConnectionPoolTest, MultipleConnections) {
    std::vector<MYSQL*> connections;
    for (int i = 0; i < 5; ++i) {
        auto conn = oj::getConnection();
        ASSERT_NE(conn, nullptr);
        connections.push_back(conn);
    }

    EXPECT_EQ(oj::ConnectionPool::getInstance().getAvailableCount(), 0);

    for (auto conn : connections) {
        oj::releaseConnection(conn);
    }

    EXPECT_EQ(oj::ConnectionPool::getInstance().getAvailableCount(), 5);
}

TEST_F(ConnectionPoolTest, ReleaseNullConnection) {
    auto before = oj::ConnectionPool::getInstance().getAvailableCount();
    oj::releaseConnection(nullptr);
    EXPECT_EQ(oj::ConnectionPool::getInstance().getAvailableCount(), before);
}

TEST_F(ConnectionPoolTest, ConcurrentAccess) {
    const int numThreads = 10;
    const int opsPerThread = 20;
    std::atomic<int> successCount{0};
    std::atomic<int> failCount{0};
    std::vector<std::thread> threads;

    for (int t = 0; t < numThreads; ++t) {
        threads.emplace_back([&]() {
            for (int i = 0; i < opsPerThread; ++i) {
                auto conn = oj::getConnection();
                if (conn) {
                    oj::releaseConnection(conn);
                    successCount.fetch_add(1);
                } else {
                    failCount.fetch_add(1);
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(oj::ConnectionPool::getInstance().getAvailableCount(), 5);
    EXPECT_EQ(successCount.load(), numThreads * opsPerThread);
}

TEST_F(ConnectionPoolTest, ConnectionIsValid) {
    auto conn = oj::getConnection();
    ASSERT_NE(conn, nullptr);

    int result = mysql_ping(conn);
    EXPECT_EQ(result, 0);

    oj::releaseConnection(conn);
}

TEST_F(ConnectionPoolTest, ConnectionCharacterSet) {
    auto conn = oj::getConnection();
    ASSERT_NE(conn, nullptr);

    const char* charset = mysql_character_set_name(conn);
    EXPECT_STREQ(charset, "utf8mb4");

    oj::releaseConnection(conn);
}

TEST_F(ConnectionPoolTest, ClosePool) {
    auto& pool = oj::ConnectionPool::getInstance();
    pool.close();

    EXPECT_EQ(pool.getAvailableCount(), 0);
}

TEST_F(ConnectionPoolTest, GetConnectionAfterClose) {
    auto& pool = oj::ConnectionPool::getInstance();
    pool.close();

    auto conn = pool.getConnection();
    EXPECT_EQ(conn, nullptr);
}

TEST_F(ConnectionPoolTest, ReleaseConnectionAfterClose) {
    auto& pool = oj::ConnectionPool::getInstance();
    pool.close();

    MYSQL* conn = mysql_init(nullptr);
    pool.releaseConnection(conn);

    EXPECT_EQ(pool.getAvailableCount(), 0);
}

}
