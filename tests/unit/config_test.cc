#include <gtest/gtest.h>
#include <fstream>
#include <filesystem>
#include "config.h"

namespace {

class ConfigTest : public ::testing::Test {
protected:
    void SetUp() override {
        oj::Config::getInstance().reset();
        tempDir = std::filesystem::temp_directory_path() / "cpp_oj_test";
        std::filesystem::create_directories(tempDir);
    }

    void TearDown() override {
        std::filesystem::remove_all(tempDir);
    }

    std::filesystem::path tempDir;
};

TEST_F(ConfigTest, LoadValidConfig) {
    std::string configContent = R"(
database:
  host: "localhost"
  port: 3306
  username: "testuser"
  password: "testpass"
  name: "testdb"

server:
  host: "127.0.0.1"
  port: 9000

connection_pool:
  size: 20

timeouts:
  request: 3000
  compile: 8000
  run: 4000

logging:
  level: "debug"
  file: "test.log"
)";

    auto configPath = tempDir / "config.yaml";
    std::ofstream(configPath) << configContent;

    oj::Config& config = oj::Config::getInstance();
    EXPECT_TRUE(config.load(configPath.string()));

    EXPECT_EQ("localhost", config.getDatabaseHost());
    EXPECT_EQ(3306, config.getDatabasePort());
    EXPECT_EQ("testuser", config.getDatabaseUsername());
    EXPECT_EQ("testpass", config.getDatabasePassword());
    EXPECT_EQ("testdb", config.getDatabaseName());

    EXPECT_EQ("127.0.0.1", config.getServerHost());
    EXPECT_EQ(9000, config.getServerPort());

    EXPECT_EQ(20, config.getConnectionPoolSize());

    EXPECT_EQ(3000, config.getRequestTimeout());
    EXPECT_EQ(8000, config.getCompileTimeout());
    EXPECT_EQ(4000, config.getRunTimeout());

    EXPECT_EQ("debug", config.getLogLevel());
    EXPECT_EQ("test.log", config.getLogFile());
}

TEST_F(ConfigTest, LoadNonExistentFile) {
    oj::Config& config = oj::Config::getInstance();
    EXPECT_FALSE(config.load("/non/existent/path.yaml"));
}

TEST_F(ConfigTest, LoadPartialConfig) {
    std::string configContent = R"(
database:
  host: "localhost"
  port: 3306
  username: "user"
  password: ""
  name: "db"

server:
  host: "0.0.0.0"
  port: 8080
)";

    auto configPath = tempDir / "partial.yaml";
    std::ofstream(configPath) << configContent;

    oj::Config& config = oj::Config::getInstance();
    EXPECT_TRUE(config.load(configPath.string()));

    EXPECT_EQ("localhost", config.getDatabaseHost());
    EXPECT_EQ(8080, config.getServerPort());
}

TEST_F(ConfigTest, SingletonPattern) {
    oj::Config& config1 = oj::Config::getInstance();
    oj::Config& config2 = oj::Config::getInstance();
    EXPECT_EQ(&config1, &config2);
}

TEST_F(ConfigTest, MultipleLoadCalls) {
    std::string configContent1 = R"(
database:
  host: "host1"
  port: 1111
  username: "user1"
  password: "pass1"
  name: "db1"

server:
  host: "0.0.0.0"
  port: 8080
)";

    std::string configContent2 = R"(
database:
  host: "host2"
  port: 2222
  username: "user2"
  password: "pass2"
  name: "db2"

server:
  host: "127.0.0.1"
  port: 9000
)";

    auto configPath1 = tempDir / "config1.yaml";
    auto configPath2 = tempDir / "config2.yaml";
    std::ofstream(configPath1) << configContent1;
    std::ofstream(configPath2) << configContent2;

    oj::Config& config = oj::Config::getInstance();
    EXPECT_TRUE(config.load(configPath1.string()));
    EXPECT_EQ("host1", config.getDatabaseHost());
    EXPECT_EQ(1111, config.getDatabasePort());

    EXPECT_TRUE(config.load(configPath2.string()));
    EXPECT_EQ("host2", config.getDatabaseHost());
    EXPECT_EQ(2222, config.getDatabasePort());
}

TEST_F(ConfigTest, LoadConfigWithEmptyOptionalSections) {
    std::string configContent = R"(
database:
  host: "localhost"
  port: 3306
  username: "user"
  password: ""
  name: "db"

server:
  host: "0.0.0.0"
  port: 8080
)";
    auto configPath = tempDir / "empty.yaml";
    std::ofstream(configPath) << configContent;

    oj::Config& config = oj::Config::getInstance();
    EXPECT_TRUE(config.load(configPath.string()));

    EXPECT_EQ("localhost", config.getDatabaseHost());
    EXPECT_EQ(8080, config.getServerPort());
}

TEST_F(ConfigTest, LoadConfigWithLargePortValue) {
    std::string configContent = R"(
database:
  host: "localhost"
  port: 65535
  username: "user"
  password: ""
  name: "db"

server:
  host: "0.0.0.0"
  port: 8080
)";
    auto configPath = tempDir / "large_port.yaml";
    std::ofstream(configPath) << configContent;

    oj::Config& config = oj::Config::getInstance();
    EXPECT_TRUE(config.load(configPath.string()));
    EXPECT_EQ(65535, config.getDatabasePort());
}

TEST_F(ConfigTest, LoadConfigWithZeroPortValue) {
    std::string configContent = R"(
database:
  host: "localhost"
  port: 0
  username: "user"
  password: ""
  name: "db"

server:
  host: "0.0.0.0"
  port: 8080
)";
    auto configPath = tempDir / "zero_port.yaml";
    std::ofstream(configPath) << configContent;

    oj::Config& config = oj::Config::getInstance();
    EXPECT_TRUE(config.load(configPath.string()));
    EXPECT_EQ(0, config.getDatabasePort());
}

TEST_F(ConfigTest, LoadConfigWithSpecialCharactersInPassword) {
    std::string configContent = R"yaml(
database:
  host: "localhost"
  port: 3306
  username: "user"
  password: "p@ss!#$%^*()"
  name: "db"

server:
  host: "0.0.0.0"
  port: 8080
)yaml";
    auto configPath = tempDir / "special_pass.yaml";
    std::ofstream(configPath) << configContent;

    oj::Config& config = oj::Config::getInstance();
    EXPECT_TRUE(config.load(configPath.string()));
    EXPECT_EQ("p@ss!#$%^*()", config.getDatabasePassword());
}

TEST_F(ConfigTest, LoadConfigWithOnlyDatabaseSection) {
    std::string configContent = R"(
database:
  host: "localhost"
  port: 3306
  username: "user"
  password: ""
  name: "db"
)";
    auto configPath = tempDir / "only_db.yaml";
    std::ofstream(configPath) << configContent;

    oj::Config& config = oj::Config::getInstance();
    EXPECT_TRUE(config.load(configPath.string()));

    EXPECT_EQ("localhost", config.getDatabaseHost());
    EXPECT_EQ(3306, config.getDatabasePort());
}

TEST_F(ConfigTest, LoadConfigWithOnlyServerSection) {
    std::string configContent = R"(
server:
  host: "127.0.0.1"
  port: 9000
)";
    auto configPath = tempDir / "only_server.yaml";
    std::ofstream(configPath) << configContent;

    oj::Config& config = oj::Config::getInstance();
    EXPECT_TRUE(config.load(configPath.string()));

    EXPECT_EQ("127.0.0.1", config.getServerHost());
    EXPECT_EQ(9000, config.getServerPort());
}

TEST_F(ConfigTest, LoadConfigAndVerifyAllGettersWork) {
    std::string configContent = R"(
database:
  host: "db.example.com"
  port: 3307
  username: "admin"
  password: "secret"
  name: "production"

server:
  host: "192.168.1.100"
  port: 8888

connection_pool:
  size: 50

timeouts:
  request: 10000
  compile: 30000
  run: 15000

logging:
  level: "error"
  file: "/var/log/oj.log"
)";
    auto configPath = tempDir / "full.yaml";
    std::ofstream(configPath) << configContent;

    oj::Config& config = oj::Config::getInstance();
    EXPECT_TRUE(config.load(configPath.string()));

    EXPECT_EQ("db.example.com", config.getDatabaseHost());
    EXPECT_EQ(3307, config.getDatabasePort());
    EXPECT_EQ("admin", config.getDatabaseUsername());
    EXPECT_EQ("secret", config.getDatabasePassword());
    EXPECT_EQ("production", config.getDatabaseName());

    EXPECT_EQ("192.168.1.100", config.getServerHost());
    EXPECT_EQ(8888, config.getServerPort());

    EXPECT_EQ(50, config.getConnectionPoolSize());

    EXPECT_EQ(10000, config.getRequestTimeout());
    EXPECT_EQ(30000, config.getCompileTimeout());
    EXPECT_EQ(15000, config.getRunTimeout());

    EXPECT_EQ("error", config.getLogLevel());
    EXPECT_EQ("/var/log/oj.log", config.getLogFile());
}

TEST_F(ConfigTest, LoadConfigWithWhitespaceInValues) {
    std::string configContent = R"(
database:
  host: " localhost "
  port: 3306
  username: " user name "
  password: " pass word "
  name: " db name "

server:
  host: " 0.0.0.0 "
  port: 8080
)";
    auto configPath = tempDir / "whitespace.yaml";
    std::ofstream(configPath) << configContent;

    oj::Config& config = oj::Config::getInstance();
    EXPECT_TRUE(config.load(configPath.string()));

    EXPECT_EQ(" localhost ", config.getDatabaseHost());
    EXPECT_EQ(" user name ", config.getDatabaseUsername());
    EXPECT_EQ(" pass word ", config.getDatabasePassword());
}

TEST_F(ConfigTest, LoadInvalidYamlFile) {
    std::string invalidContent = R"(
database:
  host: "localhost"
  port: not_a_number
  username: "user
)";
    auto configPath = tempDir / "invalid.yaml";
    std::ofstream(configPath) << invalidContent;

    oj::Config& config = oj::Config::getInstance();
    EXPECT_FALSE(config.load(configPath.string()));
}

} // namespace