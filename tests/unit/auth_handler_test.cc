#include <gtest/gtest.h>
#include <fstream>
#include <filesystem>
#include <json/json.h>
#include "auth_handler.h"
#include "user.h"
#include "connection_pool.h"
#include "config.h"
#include "logger.h"
#include "session_manager.h"
#include "../src/utils/httplib.h"

using namespace LogModule;

namespace {

class AuthHandlerTest : public ::testing::Test {
protected:
    void SetUp() override {
        tempDir = std::filesystem::temp_directory_path() / "cpp_oj_auth_handler_test";
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

        testUsername = "handler_test_user_" + std::to_string(time(nullptr));
    }

    void TearDown() override {
        auto* user = oj::User::findByUsername(testUsername);
        if (user != nullptr) {
            user->remove();
            delete user;
        }

        auto* user2 = oj::User::findByUsername("login_handler_user");
        if (user2 != nullptr) {
            user2->remove();
            delete user2;
        }

        std::filesystem::remove_all(tempDir);
    }

    std::filesystem::path tempDir;
    std::filesystem::path configFile;
    std::string testUsername;

    Json::Value parseJson(const std::string& str) {
        Json::Value json;
        Json::CharReaderBuilder builder;
        std::istringstream iss(str);
        std::string err;
        Json::parseFromStream(builder, iss, &json, &err);
        return json;
    }
};

TEST_F(AuthHandlerTest, RegisterUserInvalidJSON) {
    httplib::Request req;
    req.method = "POST";
    req.path = "/api/register";
    req.body = "not json";

    httplib::Response res;
    oj::AuthHandler::registerUser(req, res);

    EXPECT_EQ(res.status, 400);
    Json::Value json = parseJson(res.body);
    EXPECT_EQ(json["error"], "Invalid JSON");
}

TEST_F(AuthHandlerTest, RegisterUserMissingFields) {
    Json::Value body;
    body["username"] = testUsername;
    std::string bodyStr = Json::FastWriter().write(body);

    httplib::Request req;
    req.method = "POST";
    req.path = "/api/register";
    req.body = bodyStr;

    httplib::Response res;
    oj::AuthHandler::registerUser(req, res);

    EXPECT_EQ(res.status, 400);
    Json::Value json = parseJson(res.body);
    EXPECT_EQ(json["error"], "Missing username or password");
}

TEST_F(AuthHandlerTest, RegisterUserSuccess) {
    Json::Value body;
    body["username"] = testUsername;
    body["password"] = "password123";
    std::string bodyStr = Json::FastWriter().write(body);

    httplib::Request req;
    req.method = "POST";
    req.path = "/api/register";
    req.body = bodyStr;

    httplib::Response res;
    oj::AuthHandler::registerUser(req, res);

    EXPECT_EQ(res.status, 201);
    Json::Value json = parseJson(res.body);
    EXPECT_EQ(json["message"], "User registered successfully");

    auto* user = oj::User::findByUsername(testUsername);
    ASSERT_NE(user, nullptr);
    EXPECT_EQ(user->getRole(), "user");
    delete user;
    user = oj::User::findByUsername(testUsername);
    user->remove();
}

TEST_F(AuthHandlerTest, RegisterUserDuplicate) {
    {
        oj::User u;
        u.setUsername(testUsername);
        u.setPassword("password");
        u.setRole("user");
        u.save();
    }

    Json::Value body;
    body["username"] = testUsername;
    body["password"] = "newpassword";
    std::string bodyStr = Json::FastWriter().write(body);

    httplib::Request req;
    req.method = "POST";
    req.path = "/api/register";
    req.body = bodyStr;

    httplib::Response res;
    oj::AuthHandler::registerUser(req, res);

    EXPECT_EQ(res.status, 400);
    Json::Value json = parseJson(res.body);
    EXPECT_EQ(json["error"], "Username already exists");
}

TEST_F(AuthHandlerTest, LoginInvalidJSON) {
    httplib::Request req;
    req.method = "POST";
    req.path = "/api/login";
    req.body = "invalid";

    httplib::Response res;
    oj::AuthHandler::login(req, res);

    EXPECT_EQ(res.status, 400);
    Json::Value json = parseJson(res.body);
    EXPECT_EQ(json["error"], "Invalid JSON");
}

TEST_F(AuthHandlerTest, LoginMissingFields) {
    Json::Value body;
    body["username"] = "someuser";
    std::string bodyStr = Json::FastWriter().write(body);

    httplib::Request req;
    req.method = "POST";
    req.path = "/api/login";
    req.body = bodyStr;

    httplib::Response res;
    oj::AuthHandler::login(req, res);

    EXPECT_EQ(res.status, 400);
    Json::Value json = parseJson(res.body);
    EXPECT_EQ(json["error"], "Missing username or password");
}

TEST_F(AuthHandlerTest, LoginNonexistentUser) {
    Json::Value body;
    body["username"] = "nonexistent_user_xyz123";
    body["password"] = "password123";
    std::string bodyStr = Json::FastWriter().write(body);

    httplib::Request req;
    req.method = "POST";
    req.path = "/api/login";
    req.body = bodyStr;

    httplib::Response res;
    oj::AuthHandler::login(req, res);

    EXPECT_EQ(res.status, 401);
    Json::Value json = parseJson(res.body);
    EXPECT_EQ(json["error"], "Invalid username or password");
}

TEST_F(AuthHandlerTest, LoginWrongPassword) {
    {
        oj::User u;
        u.setUsername(testUsername);
        u.setPassword("correctpassword");
        u.setRole("user");
        u.save();
    }

    Json::Value body;
    body["username"] = testUsername;
    body["password"] = "wrongpassword";
    std::string bodyStr = Json::FastWriter().write(body);

    httplib::Request req;
    req.method = "POST";
    req.path = "/api/login";
    req.body = bodyStr;

    httplib::Response res;
    oj::AuthHandler::login(req, res);

    EXPECT_EQ(res.status, 401);
    Json::Value json = parseJson(res.body);
    EXPECT_EQ(json["error"], "Invalid username or password");
}

TEST_F(AuthHandlerTest, LoginSuccess) {
    {
        oj::User u;
        u.setUsername(testUsername);
        u.setPassword("correctpassword");
        u.setRole("user");
        u.save();
    }

    Json::Value body;
    body["username"] = testUsername;
    body["password"] = "correctpassword";
    std::string bodyStr = Json::FastWriter().write(body);

    httplib::Request req;
    req.method = "POST";
    req.path = "/api/login";
    req.body = bodyStr;

    httplib::Response res;
    oj::AuthHandler::login(req, res);

    EXPECT_EQ(res.status, 200);
    Json::Value json = parseJson(res.body);
    EXPECT_EQ(json["message"], "Login successful");

    EXPECT_TRUE(res.has_header("Set-Cookie"));
    std::string cookie = res.get_header_value("Set-Cookie");
    EXPECT_TRUE(cookie.find("oj_session=") != std::string::npos);
}

TEST_F(AuthHandlerTest, LoginSetsHttpOnlyCookie) {
    {
        oj::User u;
        u.setUsername(testUsername);
        u.setPassword("password");
        u.setRole("user");
        u.save();
    }

    Json::Value body;
    body["username"] = testUsername;
    body["password"] = "password";
    std::string bodyStr = Json::FastWriter().write(body);

    httplib::Request req;
    req.method = "POST";
    req.path = "/api/login";
    req.body = bodyStr;

    httplib::Response res;
    oj::AuthHandler::login(req, res);

    EXPECT_EQ(res.status, 200);
    std::string cookie = res.get_header_value("Set-Cookie");
    EXPECT_TRUE(cookie.find("HttpOnly") != std::string::npos);
    EXPECT_TRUE(cookie.find("SameSite=Strict") != std::string::npos);
}

TEST_F(AuthHandlerTest, LogoutClearsCookie) {
    httplib::Request req;
    req.method = "POST";
    req.path = "/api/logout";
    req.set_header("Cookie", "oj_session=test_token_123");

    httplib::Response res;
    oj::AuthHandler::logout(req, res);

    EXPECT_EQ(res.status, 200);
    Json::Value json = parseJson(res.body);
    EXPECT_EQ(json["message"], "Logout successful");

    EXPECT_TRUE(res.has_header("Set-Cookie"));
    std::string cookie = res.get_header_value("Set-Cookie");
    EXPECT_TRUE(cookie.find("Max-Age=0") != std::string::npos);
}

TEST_F(AuthHandlerTest, LogoutWithoutCookie) {
    httplib::Request req;
    req.method = "POST";
    req.path = "/api/logout";

    httplib::Response res;
    oj::AuthHandler::logout(req, res);

    EXPECT_EQ(res.status, 200);
    Json::Value json = parseJson(res.body);
    EXPECT_EQ(json["message"], "Logout successful");
}

TEST_F(AuthHandlerTest, LogoutDestroysSession) {
    {
        oj::User u;
        u.setUsername(testUsername);
        u.setPassword("password");
        u.setRole("user");
        u.save();
    }

    Json::Value loginBody;
    loginBody["username"] = testUsername;
    loginBody["password"] = "password";
    std::string loginBodyStr = Json::FastWriter().write(loginBody);

    httplib::Request loginReq;
    loginReq.method = "POST";
    loginReq.path = "/api/login";
    loginReq.body = loginBodyStr;

    httplib::Response loginRes;
    oj::AuthHandler::login(loginReq, loginRes);

    ASSERT_EQ(loginRes.status, 200);
    std::string cookie = loginRes.get_header_value("Set-Cookie");
    ASSERT_TRUE(cookie.find("oj_session=") != std::string::npos);

    size_t start = cookie.find("oj_session=") + strlen("oj_session=");
    size_t end = cookie.find(';', start);
    std::string token = cookie.substr(start, end - start);

    auto& sessionManager = oj::SessionManager::getInstance();
    ASSERT_NE(sessionManager.getSession(token), nullptr);

    httplib::Request logoutReq;
    logoutReq.method = "POST";
    logoutReq.path = "/api/logout";
    logoutReq.set_header("Cookie", "oj_session=" + token);

    httplib::Response logoutRes;
    oj::AuthHandler::logout(logoutReq, logoutRes);

    EXPECT_EQ(logoutRes.status, 200);
    EXPECT_EQ(sessionManager.getSession(token), nullptr);
}

TEST_F(AuthHandlerTest, LogoutWithInvalidToken) {
    auto& sessionManager = oj::SessionManager::getInstance();
    ASSERT_EQ(sessionManager.getSession("invalid_token_xyz"), nullptr);

    httplib::Request req;
    req.method = "POST";
    req.path = "/api/logout";
    req.set_header("Cookie", "oj_session=invalid_token_xyz");

    httplib::Response res;
    oj::AuthHandler::logout(req, res);

    EXPECT_EQ(res.status, 200);
    Json::Value json = parseJson(res.body);
    EXPECT_EQ(json["message"], "Logout successful");
}

TEST_F(AuthHandlerTest, LogoutWithOtherCookiesPresent) {
    httplib::Request req;
    req.method = "POST";
    req.path = "/api/logout";
    req.set_header("Cookie", "other_cookie=value; oj_session=test_token_456; another_cookie=foo");

    httplib::Response res;
    oj::AuthHandler::logout(req, res);

    EXPECT_EQ(res.status, 200);
    EXPECT_TRUE(res.has_header("Set-Cookie"));
}

TEST_F(AuthHandlerTest, LogoutTwice) {
    {
        oj::User u;
        u.setUsername("logout_double_test_user");
        u.setPassword("password");
        u.setRole("user");
        u.save();
    }

    Json::Value loginBody;
    loginBody["username"] = "logout_double_test_user";
    loginBody["password"] = "password";
    std::string loginBodyStr = Json::FastWriter().write(loginBody);

    httplib::Request loginReq;
    loginReq.method = "POST";
    loginReq.path = "/api/login";
    loginReq.body = loginBodyStr;

    httplib::Response loginRes;
    oj::AuthHandler::login(loginReq, loginRes);

    ASSERT_EQ(loginRes.status, 200);
    std::string cookie = loginRes.get_header_value("Set-Cookie");
    size_t start = cookie.find("oj_session=") + strlen("oj_session=");
    size_t end = cookie.find(';', start);
    std::string token = cookie.substr(start, end - start);

    auto& sessionManager = oj::SessionManager::getInstance();
    ASSERT_NE(sessionManager.getSession(token), nullptr);

    httplib::Request logoutReq1;
    logoutReq1.method = "POST";
    logoutReq1.path = "/api/logout";
    logoutReq1.set_header("Cookie", "oj_session=" + token);

    httplib::Response logoutRes1;
    oj::AuthHandler::logout(logoutReq1, logoutRes1);

    EXPECT_EQ(logoutRes1.status, 200);
    EXPECT_EQ(sessionManager.getSession(token), nullptr);

    httplib::Request logoutReq2;
    logoutReq2.method = "POST";
    logoutReq2.path = "/api/logout";
    logoutReq2.set_header("Cookie", "oj_session=" + token);

    httplib::Response logoutRes2;
    oj::AuthHandler::logout(logoutReq2, logoutRes2);

    EXPECT_EQ(logoutRes2.status, 200);

    auto* user = oj::User::findByUsername("logout_double_test_user");
    if (user != nullptr) {
        user->remove();
        delete user;
    }
}

TEST_F(AuthHandlerTest, LogoutSetsCorrectCookieAttributes) {
    httplib::Request req;
    req.method = "POST";
    req.path = "/api/logout";
    req.set_header("Cookie", "oj_session=test_token");

    httplib::Response res;
    oj::AuthHandler::logout(req, res);

    EXPECT_EQ(res.status, 200);
    std::string cookie = res.get_header_value("Set-Cookie");
    EXPECT_TRUE(cookie.find("oj_session=") != std::string::npos);
    EXPECT_TRUE(cookie.find("Max-Age=0") != std::string::npos);
    EXPECT_TRUE(cookie.find("Path=/") != std::string::npos);
    EXPECT_TRUE(cookie.find("HttpOnly") != std::string::npos);
    EXPECT_TRUE(cookie.find("SameSite=Strict") != std::string::npos);
}

TEST_F(AuthHandlerTest, RegisterAndLoginWorkflow) {
    {
        std::random_device rd;
        std::string uniqueId = std::to_string(rd()) + "_" + std::to_string(time(nullptr));
        std::string username = "workflow_user_" + uniqueId;
        
        Json::Value regBody;
        regBody["username"] = username;
        regBody["password"] = "securepass123";
        std::string regBodyStr = Json::FastWriter().write(regBody);

        httplib::Request regReq;
        regReq.method = "POST";
        regReq.path = "/api/register";
        regReq.body = regBodyStr;

        httplib::Response regRes;
        oj::AuthHandler::registerUser(regReq, regRes);

        EXPECT_EQ(regRes.status, 201);

        Json::Value loginBody;
        loginBody["username"] = username;
        loginBody["password"] = "securepass123";
        std::string loginBodyStr = Json::FastWriter().write(loginBody);

        httplib::Request loginReq;
        loginReq.method = "POST";
        loginReq.path = "/api/login";
        loginReq.body = loginBodyStr;

        httplib::Response loginRes;
        oj::AuthHandler::login(loginReq, loginRes);

        EXPECT_EQ(loginRes.status, 200);
        EXPECT_TRUE(loginRes.has_header("Set-Cookie"));
    }
}

} // namespace
