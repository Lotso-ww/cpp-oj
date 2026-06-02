#include "problem_handler.h"
#include "../service/problem_service.h"
#include "../utils/logger.h"
#include <json/json.h>
#include <sstream>

using namespace LogModule;

namespace oj {

void ProblemHandler::listProblems(const httplib::Request& req, httplib::Response& res) {
    (void)req;
    Json::Value result = ProblemService::getAllProblems();
    res.status = 200;
    res.set_content(Json::FastWriter().write(result), "application/json");
}

void ProblemHandler::getProblem(const httplib::Request& req, httplib::Response& res) {
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
    
    Json::Value result = ProblemService::getProblemDetail(id);
    if (result.empty()) {
        res.status = 404;
        Json::Value error;
        error["error"] = "Problem not found";
        res.set_content(Json::FastWriter().write(error), "application/json");
        return;
    }
    
    res.status = 200;
    res.set_content(Json::FastWriter().write(result), "application/json");
}

}