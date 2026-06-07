#include "submit_handler.h"
#include "../model/problem.h"
#include "../model/test_case.h"
#include "../service/executor_service.h"
#include "../utils/logger.h"
#include <json/json.h>
#include <sstream>

using namespace LogModule;

namespace oj {

void SubmitHandler::submitCode(const httplib::Request& req, httplib::Response& res) {
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

    if (!root.isMember("code") || !root.isMember("problemId")) {
        res.status = 400;
        Json::Value error;
        error["error"] = "Missing required fields: code, problemId";
        res.set_content(Json::FastWriter().write(error), "application/json");
        return;
    }

    std::string code = root["code"].asString();
    int problemId = root["problemId"].asInt();

    if (code.empty()) {
        res.status = 400;
        Json::Value error;
        error["error"] = "Code cannot be empty";
        res.set_content(Json::FastWriter().write(error), "application/json");
        return;
    }

    Problem* problem = Problem::findById(problemId);
    if (!problem) {
        res.status = 404;
        Json::Value error;
        error["error"] = "Problem not found";
        res.set_content(Json::FastWriter().write(error), "application/json");
        return;
    }

    const std::vector<TestCase>& testCases = problem->getTestCases();
    if (testCases.empty()) {
        res.status = 400;
        Json::Value error;
        error["error"] = "No test cases configured for this problem";
        res.set_content(Json::FastWriter().write(error), "application/json");
        delete problem;
        return;
    }

    std::vector<std::pair<std::string, std::string>> cases;
    for (const auto& tc : testCases) {
        cases.emplace_back(tc.getInput(), tc.getExpected());
    }

    delete problem;

    auto response = ExecutorService::getInstance().compileAndRun(code, cases);

    Json::Value result;
    if (!response.compileSuccess) {
        result["status"] = "CE";
        result["compileOutput"] = response.compileOutput;
        result["error"] = response.errorMessage;
    } else {
        if (response.errorMessage == "Wrong Answer") {
            result["status"] = "WA";
        } else {
            switch (response.result) {
                case RunResult::SUCCESS:
                    result["status"] = "AC";
                    break;
                case RunResult::RUNTIME_ERROR:
                    result["status"] = "RE";
                    result["stderr"] = response.stderr;
                    break;
                case RunResult::TIME_LIMIT_EXCEEDED:
                    result["status"] = "TLE";
                    break;
                case RunResult::MEMORY_LIMIT_EXCEEDED:
                    result["status"] = "MLE";
                    break;
                default:
                    result["status"] = "SYSTEM_ERROR";
                    result["error"] = response.errorMessage;
                    break;
            }
        }
        result["stdout"] = response.stdout;
        result["executionTimeMs"] = response.executionTimeMs;
        if (!response.errorMessage.empty() && response.result != RunResult::SUCCESS) {
            result["error"] = response.errorMessage;
        }
    }

    res.status = 200;
    res.set_content(Json::FastWriter().write(result), "application/json");
}

}