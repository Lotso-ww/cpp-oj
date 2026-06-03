#include "auth_handler.h"
#include "../service/auth_service.h"
#include "../service/session_manager.h"
#include "../utils/logger.h"
#include <json/json.h>

namespace oj {

static const char* SESSION_COOKIE_NAME = "oj_session";

void AuthHandler::login(const httplib::Request& req, httplib::Response& res) {
    Json::Value json;
    Json::Reader reader;

    if (!reader.parse(req.body, json) || !json.isObject()) {
        res.status = 400;
        res.set_content(R"({"error":"Invalid JSON"})", "application/json");
        return;
    }

    if (!json.isMember("username") || !json.isMember("password")) {
        res.status = 400;
        res.set_content(R"({"error":"Missing username or password"})", "application/json");
        return;
    }

    std::string username = json["username"].asString();
    std::string password = json["password"].asString();

    int userId = 0;
    std::string role;

    if (!AuthService::login(username, password, userId, role)) {
        res.status = 401;
        res.set_content(R"({"error":"Invalid username or password"})", "application/json");
        return;
    }

    std::string token = AuthService::getSessionToken(userId, username, role);

    res.status = 200;
    res.set_header("Set-Cookie", std::string(SESSION_COOKIE_NAME) + "=" + token + "; Path=/; HttpOnly; SameSite=Strict");
    res.set_content(R"({"message":"Login successful"})", "application/json");
}

void AuthHandler::logout(const httplib::Request& req, httplib::Response& res) {
    const auto& cookie = req.get_header_value("Cookie");

    std::string token;
    size_t pos = cookie.find(SESSION_COOKIE_NAME);
    if (pos != std::string::npos) {
        size_t start = pos + strlen(SESSION_COOKIE_NAME) + 1;
        size_t end = cookie.find(';', start);
        token = cookie.substr(start, end - start);
    }

    if (!token.empty()) {
        AuthService::logout(token);
    }

    res.status = 200;
    res.set_header("Set-Cookie", std::string(SESSION_COOKIE_NAME) + "=; Path=/; HttpOnly; SameSite=Strict; Max-Age=0");
    res.set_content(R"({"message":"Logout successful"})", "application/json");
}

void AuthHandler::registerUser(const httplib::Request& req, httplib::Response& res) {
    Json::Value json;
    Json::Reader reader;

    if (!reader.parse(req.body, json) || !json.isObject()) {
        res.status = 400;
        res.set_content(R"({"error":"Invalid JSON"})", "application/json");
        return;
    }

    if (!json.isMember("username") || !json.isMember("password")) {
        res.status = 400;
        res.set_content(R"({"error":"Missing username or password"})", "application/json");
        return;
    }

    std::string username = json["username"].asString();
    std::string password = json["password"].asString();

    std::string error;
    if (!AuthService::registerUser(username, password, error)) {
        res.status = 400;
        res.set_content("{\"error\":\"" + error + "\"}", "application/json");
        return;
    }

    res.status = 201;
    res.set_content(R"({"message":"User registered successfully"})", "application/json");
}

}
