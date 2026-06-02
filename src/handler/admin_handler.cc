#include "admin_handler.h"
#include "../model/problem.h"
#include "../model/test_case.h"
#include "../db/connection_pool.h"
#include "../utils/logger.h"
#include <json/json.h>
#include <sstream>

using namespace LogModule;

namespace oj {

void AdminHandler::createProblem(const httplib::Request& req, httplib::Response& res) {
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