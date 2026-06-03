#include <gtest/gtest.h>
#include <random>
#include <fstream>
#include <filesystem>
#include <json/json.h>
#include "auth_service.h"
#include "session_manager.h"
#include "user.h"
#include "connection_pool.h"
#include "config.h"
#include "logger.h"

using namespace LogModule;

namespace {

class AuthServiceTest : public ::testing::Test {
protected:
    void SetUp() override {
        tempDir = std::filesystem::temp_directory_path() / "cpp_oj_auth_service_test";
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

        std::random_device rd;
        std::string randomSuffix = std::to_string(rd()) + "_" + std::to_string(time(nullptr)) + "_" + std::to_string(pthread_self());
        testUsername = "auth_test_user_" + randomSuffix;
    }

    void TearDown() override {
        auto* user = oj::User::findByUsername(testUsername);
        if (user != nullptr) {
            user->remove();
            delete user;
        }

        auto* user2 = oj::User::findByUsername("nonexistent_auth_user");
        if (user2 != nullptr) {
            user2->remove();
            delete user2;
        }

        std::filesystem::remove_all(tempDir);
    }

    std::filesystem::path tempDir;
    std::filesystem::path configFile;
    std::string testUsername;
};

TEST_F(AuthServiceTest, RegisterUserSuccess) {
    std::string error;
    bool result = oj::AuthService::registerUser(testUsername, "password123", error);

    EXPECT_TRUE(result);
    EXPECT_TRUE(error.empty());

    auto* user = oj::User::findByUsername(testUsername);
    ASSERT_NE(user, nullptr);
    EXPECT_EQ(user->getUsername(), testUsername);
    EXPECT_EQ(user->getRole(), "user");
    delete user;
    user = oj::User::findByUsername(testUsername);
    user->remove();
}

TEST_F(AuthServiceTest, RegisterUserDuplicateUsername) {
    oj::User u;
    u.setUsername(testUsername);
    u.setPassword("original");
    u.setRole("user");
    int id = u.save();
    ASSERT_GT(id, 0);

    std::string error;
    bool result = oj::AuthService::registerUser(testUsername, "newpassword", error);

    EXPECT_FALSE(result);
    EXPECT_EQ(error, "Username already exists");
}

TEST_F(AuthServiceTest, RegisterUserEmptyUsername) {
    std::string error;
    bool result = oj::AuthService::registerUser("", "password123", error);

    EXPECT_FALSE(result);
    EXPECT_EQ(error, "Username and password cannot be empty");
}

TEST_F(AuthServiceTest, RegisterUserEmptyPassword) {
    std::string error;
    bool result = oj::AuthService::registerUser("someuser", "", error);

    EXPECT_FALSE(result);
    EXPECT_EQ(error, "Username and password cannot be empty");
}

TEST_F(AuthServiceTest, RegisterUserUsernameTooShort) {
    std::string error;
    bool result = oj::AuthService::registerUser("ab", "password123", error);

    EXPECT_FALSE(result);
    EXPECT_EQ(error, "Username must be between 3 and 64 characters");
}

TEST_F(AuthServiceTest, RegisterUserUsernameTooLong) {
    std::string longUsername(100, 'a');
    std::string error;
    bool result = oj::AuthService::registerUser(longUsername, "password123", error);

    EXPECT_FALSE(result);
    EXPECT_EQ(error, "Username must be between 3 and 64 characters");
}

TEST_F(AuthServiceTest, RegisterUserPasswordTooShort) {
    std::string error;
    bool result = oj::AuthService::registerUser("validuser", "12345", error);

    EXPECT_FALSE(result);
    EXPECT_EQ(error, "Password must be at least 6 characters");
}

TEST_F(AuthServiceTest, LoginSuccess) {
    {
        oj::User u;
        u.setUsername(testUsername);
        u.setPassword("correctpassword");
        u.setRole("admin");
        u.save();
    }

    int userId = 0;
    std::string role;
    bool result = oj::AuthService::login(testUsername, "correctpassword", userId, role);

    EXPECT_TRUE(result);
    EXPECT_GT(userId, 0);
    EXPECT_EQ(role, "admin");
}

TEST_F(AuthServiceTest, LoginFailureWrongPassword) {
    {
        oj::User u;
        u.setUsername(testUsername);
        u.setPassword("correctpassword");
        u.setRole("user");
        u.save();
    }

    int userId = 0;
    std::string role;
    bool result = oj::AuthService::login(testUsername, "wrongpassword", userId, role);

    EXPECT_FALSE(result);
    EXPECT_EQ(userId, 0);
}

TEST_F(AuthServiceTest, LoginFailureNonexistentUser) {
    int userId = 0;
    std::string role;
    bool result = oj::AuthService::login("nonexistent_auth_user", "password", userId, role);

    EXPECT_FALSE(result);
    EXPECT_EQ(userId, 0);
}

TEST_F(AuthServiceTest, GetSessionTokenCreatesSession) {
    std::string token = oj::AuthService::getSessionToken(42, "testuser", "admin");

    EXPECT_FALSE(token.empty());
    EXPECT_EQ(token.size(), 32U);

    auto* session = oj::SessionManager::getInstance().getSession(token);
    ASSERT_NE(session, nullptr);
    EXPECT_EQ(session->getUserId(), 42);
    EXPECT_EQ(session->getUsername(), "testuser");
    EXPECT_EQ(session->getRole(), "admin");

    oj::SessionManager::getInstance().destroySession(token);
}

TEST_F(AuthServiceTest, ValidateSessionSuccess) {
    std::string token = oj::AuthService::getSessionToken(100, "validuser", "user");

    int userId = 0;
    std::string username;
    std::string role;
    bool result = oj::AuthService::validateSession(token, userId, username, role);

    EXPECT_TRUE(result);
    EXPECT_EQ(userId, 100);
    EXPECT_EQ(username, "validuser");
    EXPECT_EQ(role, "user");

    oj::SessionManager::getInstance().destroySession(token);
}

TEST_F(AuthServiceTest, ValidateSessionFailureInvalidToken) {
    int userId = 0;
    std::string username;
    std::string role;
    bool result = oj::AuthService::validateSession("invalid_token_123", userId, username, role);

    EXPECT_FALSE(result);
    EXPECT_EQ(userId, 0);
    EXPECT_TRUE(username.empty());
    EXPECT_TRUE(role.empty());
}

TEST_F(AuthServiceTest, LogoutDestroysSession) {
    std::string token = oj::AuthService::getSessionToken(1, "user", "user");

    bool result = oj::AuthService::logout(token);
    EXPECT_TRUE(result);

    auto* session = oj::SessionManager::getInstance().getSession(token);
    EXPECT_EQ(session, nullptr);
}

TEST_F(AuthServiceTest, LogoutFailureInvalidToken) {
    bool result = oj::AuthService::logout("nonexistent_token");
    EXPECT_FALSE(result);
}

TEST_F(AuthServiceTest, RegisterAndLoginWorkflow) {
    std::string registerError;
    std::string randomSuffix = std::to_string(std::random_device{}()) + "_" + std::to_string(time(nullptr));
    std::string newUsername = "newloginuser_" + randomSuffix;
    bool registered = oj::AuthService::registerUser(newUsername, "securepass123", registerError);
    ASSERT_TRUE(registered) << registerError;

    int userId = 0;
    std::string role;
    bool loggedIn = oj::AuthService::login(newUsername, "securepass123", userId, role);
    ASSERT_TRUE(loggedIn);
    EXPECT_GT(userId, 0);
    EXPECT_EQ(role, "user");

    auto* user = oj::User::findByUsername(newUsername);
    ASSERT_NE(user, nullptr);
    delete user;
    user = oj::User::findByUsername(newUsername);
    user->remove();
}

TEST_F(AuthServiceTest, LoginAfterLogoutSessionInvalid) {
    std::string token = oj::AuthService::getSessionToken(1, "user", "user");

    bool logoutResult = oj::AuthService::logout(token);
    EXPECT_TRUE(logoutResult);

    int userId = 0;
    std::string username;
    std::string role;
    bool valid = oj::AuthService::validateSession(token, userId, username, role);

    EXPECT_FALSE(valid);
    EXPECT_TRUE(username.empty());
    EXPECT_TRUE(role.empty());
}

TEST_F(AuthServiceTest, MultipleSessionsPerUser) {
    std::string token1 = oj::AuthService::getSessionToken(1, "user", "user");
    std::string token2 = oj::AuthService::getSessionToken(1, "user", "user");
    std::string token3 = oj::AuthService::getSessionToken(1, "user", "user");

    EXPECT_NE(token1, token2);
    EXPECT_NE(token2, token3);

    oj::SessionManager::getInstance().destroySession(token1);
    oj::SessionManager::getInstance().destroySession(token2);
    oj::SessionManager::getInstance().destroySession(token3);
}

TEST_F(AuthServiceTest, SessionContainsCorrectUserData) {
    std::string token = oj::AuthService::getSessionToken(999, "specialuser", "admin");

    int userId = 0;
    std::string username;
    std::string role;
    oj::AuthService::validateSession(token, userId, username, role);

    EXPECT_EQ(userId, 999);
    EXPECT_EQ(username, "specialuser");
    EXPECT_EQ(role, "admin");

    oj::SessionManager::getInstance().destroySession(token);
}

} // namespace
