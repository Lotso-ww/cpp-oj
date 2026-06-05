#include "admin_handler.h"
#include "../model/problem.h"
#include "../model/test_case.h"
#include "../db/connection_pool.h"
#include "../service/auth_service.h"
#include "../utils/logger.h"
#include <json/json.h>
#include <sstream>

using namespace LogModule;

namespace oj {

static const char* SESSION_COOKIE_NAME = "oj_session";

static bool checkAdminAuth(const httplib::Request& req, httplib::Response& res) {
    const auto& cookie = req.get_header_value("Cookie");
    std::string token;
    size_t pos = cookie.find(SESSION_COOKIE_NAME);
    if (pos != std::string::npos) {
        size_t start = pos + strlen(SESSION_COOKIE_NAME) + 1;
        size_t end = cookie.find(';', start);
        token = cookie.substr(start, end - start);
    }

    if (token.empty()) {
        res.status = 401;
        Json::Value error;
        error["error"] = "Unauthorized";
        res.set_content(Json::FastWriter().write(error), "application/json");
        return false;
    }

    int userId = 0;
    std::string username, role;
    if (!AuthService::validateSession(token, userId, username, role)) {
        res.status = 401;
        Json::Value error;
        error["error"] = "Unauthorized";
        res.set_content(Json::FastWriter().write(error), "application/json");
        return false;
    }

    if (role != "admin") {
        res.status = 403;
        Json::Value error;
        error["error"] = "Forbidden";
        res.set_content(Json::FastWriter().write(error), "application/json");
        return false;
    }

    return true;
}

void AdminHandler::createProblem(const httplib::Request& req, httplib::Response& res) {
    if (!checkAdminAuth(req, res)) {
        return;
    }

    Json::Value root;
    Json::CharReaderBuilder builder;
    std::istringstream iss(req.body);
    std::string err;
    if (!Json::parseFromStream(builder, iss, &root, &err)) {
        res.status = 400;
        Json::Value error;
        error["error"] = "Invalid JSON";
        res.set_content(Json::FastWriter().write(error), "application/json");
        return;
    }
    
    if (!root.isMember("title") || !root.isMember("difficulty") || !root.isMember("content")) {
        res.status = 400;
        Json::Value error;
        error["error"] = "Missing required fields: title, difficulty, content";
        res.set_content(Json::FastWriter().write(error), "application/json");
        return;
    }
    
    std::string title = root["title"].asString();
    std::string difficulty = root["difficulty"].asString();
    std::string content = root["content"].asString();
    std::string templateCode = root.isMember("template") ? root["template"].asString() : "";
    
    if (title.empty() || difficulty.empty() || content.empty()) {
        res.status = 400;
        Json::Value error;
        error["error"] = "Missing required fields: title, difficulty, content";
        res.set_content(Json::FastWriter().write(error), "application/json");
        return;
    }
    
    if (difficulty != "Easy" && difficulty != "Medium" && difficulty != "Hard") {
        res.status = 400;
        Json::Value error;
        error["error"] = "Invalid difficulty. Must be Easy, Medium, or Hard";
        res.set_content(Json::FastWriter().write(error), "application/json");
        return;
    }
    
    Problem problem;
    problem.setTitle(title);
    problem.setDifficulty(difficulty);
    problem.setContent(content);
    problem.setTemplate(templateCode);
    
    int problemId = problem.save();
    if (problemId < 0) {
        res.status = 500;
        Json::Value error;
        error["error"] = "Failed to create problem";
        res.set_content(Json::FastWriter().write(error), "application/json");
        return;
    }
    
    if (root.isMember("testCases") && root["testCases"].isArray()) {
        const Json::Value& testCases = root["testCases"];
        for (Json::ArrayIndex i = 0; i < testCases.size(); ++i) {
            const Json::Value& tc = testCases[i];
            if (tc.isMember("input") && tc.isMember("expected")) {
                TestCase testCase;
                testCase.setProblemId(problemId);
                testCase.setInput(tc["input"].asString());
                testCase.setExpected(tc["expected"].asString());
                testCase.setPosition(static_cast<int>(i));
                testCase.save();
            }
        }
    }
    
    Json::Value response;
    response["id"] = problemId;
    response["title"] = title;
    res.status = 201;
    res.set_content(Json::FastWriter().write(response), "application/json");
}

void AdminHandler::deleteProblem(const httplib::Request& req, httplib::Response& res) {
    if (!checkAdminAuth(req, res)) {
        return;
    }

    auto it = req.path_params.find("id");
    if (it == req.path_params.end()) {
        res.status = 400;
        Json::Value error;
        error["error"] = "Invalid problem ID";
        res.set_content(Json::FastWriter().write(error), "application/json");
        return;
    }
    
    int id = 0;
    std::istringstream iss(it->second);
    if (!(iss >> id) || id <= 0) {
        res.status = 400;
        Json::Value error;
        error["error"] = "Invalid problem ID";
        res.set_content(Json::FastWriter().write(error), "application/json");
        return;
    }
    
    Problem* problem = Problem::findById(id);
    if (!problem) {
        res.status = 404;
        Json::Value error;
        error["error"] = "Problem not found";
        res.set_content(Json::FastWriter().write(error), "application/json");
        return;
    }
    
    if (!problem->remove()) {
        res.status = 500;
        Json::Value error;
        error["error"] = "Failed to delete problem";
        res.set_content(Json::FastWriter().write(error), "application/json");
        delete problem;
        return;
    }
    
    Json::Value response;
    response["message"] = "Problem deleted successfully";
    res.status = 200;
    res.set_content(Json::FastWriter().write(response), "application/json");
    delete problem;
}

}