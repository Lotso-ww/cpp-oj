#include "run_handler.h"
#include "../service/executor_service.h"
#include "../model/problem.h"
#include "../model/test_case.h"
#include "../utils/logger.h"
#include <json/json.h>
#include <sstream>

namespace oj {

void RunHandler::runCode(const httplib::Request& req, httplib::Response& res) {
    Json::Value json;
    Json::CharReaderBuilder builder;
    std::istringstream iss(req.body);
    std::string err;
    if (!Json::parseFromStream(builder, iss, &json, &err)) {
        res.status = 400;
        Json::Value error;
        error["error"] = "Invalid JSON";
        res.set_content(Json::FastWriter().write(error), "application/json");
        return;
    }

    if (!json.isMember("code") || !json.isMember("problemId")) {
        res.status = 400;
        Json::Value error;
        error["error"] = "Missing required fields: code, problemId";
        res.set_content(Json::FastWriter().write(error), "application/json");
        return;
    }

    std::string code = json["code"].asString();
    int problemId = json["problemId"].asInt();

    if (code.empty()) {
        res.status = 400;
        Json::Value error;
        error["error"] = "Code cannot be empty";
        res.set_content(Json::FastWriter().write(error), "application/json");
        return;
    }

    // Resolve the test cases to run:
    //   - If the request includes a "cases" array, use those (user-edited
    //     cases from the editor, e.g. added/removed/modified by the user).
    //   - Otherwise, fall back to the problem's default test cases from the DB.
    std::vector<std::pair<std::string, std::string>> cases;
    bool casesFromRequest = false;

    if (json.isMember("cases") && json["cases"].isArray()) {
        casesFromRequest = true;
        for (const auto& c : json["cases"]) {
            std::string input    = c.isMember("input")    ? c["input"].asString()    : "";
            std::string expected = c.isMember("expected") ? c["expected"].asString() : "";
            cases.emplace_back(input, expected);
        }
    } else {
        Problem* problem = Problem::findById(problemId);
        if (!problem) {
            res.status = 404;
            Json::Value error;
            error["error"] = "Problem not found";
            res.set_content(Json::FastWriter().write(error), "application/json");
            return;
        }
        const std::vector<TestCase>& testCases = problem->getTestCases();
        for (const auto& tc : testCases) {
            cases.emplace_back(tc.getInput(), tc.getExpected());
        }
        delete problem;
    }

    if (cases.empty()) {
        res.status = 400;
        Json::Value error;
        error["error"] = casesFromRequest
            ? "请添加至少一个测试用例"
            : "No test cases configured for this problem";
        res.set_content(Json::FastWriter().write(error), "application/json");
        return;
    }

    // Run each test case individually. This compiles the code once per case
    // (less efficient than a single compile + N runs, but keeps the change
    // small — the executor's compileAndRun returns an overall result, not
    // per-case. For 3-5 test cases the overhead is acceptable for an
    // internal tool; if it becomes a bottleneck, we can refactor the
    // executor to return a vector of per-case results in one call).
    Json::Value casesArr(Json::arrayValue);
    int passed = 0;
    int total = 0;
    bool compileOk = true;
    std::string compileOut;
    bool stoppedEarly = false;

    for (size_t i = 0; i < cases.size(); ++i) {
        if (stoppedEarly) break;

        const std::string& input    = cases[i].first;
        const std::string& expected = cases[i].second;

        std::vector<std::pair<std::string, std::string>> singleCase = {
            {input, expected}
        };
        auto response = ExecutorService::getInstance().compileAndRun(code, singleCase);

        Json::Value caseObj;
        caseObj["position"]        = (int)i;
        caseObj["input"]           = input;
        caseObj["expected"]        = expected;
        caseObj["actual"]          = response.stdout;
        caseObj["stderr"]          = response.stderr;
        caseObj["executionTimeMs"] = (int)response.executionTimeMs;
        caseObj["compileSuccess"]  = response.compileSuccess;
        caseObj["compileOutput"]   = response.compileOutput;
        caseObj["errorMessage"]    = response.errorMessage;

        std::string status;
        if (!response.compileSuccess) {
            status = "CE";
            compileOk = false;
            compileOut = response.compileOutput;
            stoppedEarly = true;  // no point running more cases after a CE
        } else if (response.result == RunResult::TIME_LIMIT_EXCEEDED) {
            status = "TLE";
        } else if (response.result == RunResult::RUNTIME_ERROR) {
            status = "RE";
        } else if (response.result == RunResult::MEMORY_LIMIT_EXCEEDED) {
            status = "MLE";
        } else if (response.result == RunResult::SYSTEM_ERROR) {
            status = "SYSTEM_ERROR";
        } else if (response.errorMessage == "Wrong Answer") {
            // Executor signals a per-case WA via this errorMessage while
            // still returning RunResult::SUCCESS. (See executor_service.cc.)
            status = "WA";
        } else {
            status = "AC";
            passed++;
        }
        caseObj["status"] = status;
        casesArr.append(caseObj);
        total++;
    }

    Json::Value result;
    result["compileSuccess"] = compileOk;
    result["compileOutput"]  = compileOut;
    result["cases"]          = casesArr;
    result["passed"]         = passed;
    result["total"]          = total;
    result["allPassed"]      = (passed == total);

    res.status = 200;
    res.set_content(Json::FastWriter().write(result), "application/json");
}

}
