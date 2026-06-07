#include "run_handler.h"
#include "../service/executor_service.h"
#include "../model/problem.h"
#include "../model/test_case.h"
#include "../utils/logger.h"
#include <json/json.h>
#include <sstream>

namespace oj {

// Internal record representing a single case the executor should run, with
// the metadata we need to label the result. `isCustom` distinguishes a
// LeetCode-style user-added case (no expected output) from an official
// problem case (compared against expected).
struct RunCase {
    std::string input;
    std::string expected;   // empty for custom cases
    bool        isCustom;
};

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

    // Resolve the test cases to run. There are three shapes of request:
    //   1. `cases` (legacy): user-provided test cases with expected output.
    //      These REPLACE the DB cases — used by the old "edit-then-run" UI
    //      and by the integration tests.
    //   2. `customCases` (new): LeetCode-style user-added test cases with
    //      input only (no expected). These are APPENDED to the problem's
    //      official DB cases, so the user gets a unified per-case result
    //      covering both authoritative and exploratory cases.
    //   3. Neither: just run the problem's DB cases.
    std::vector<RunCase> cases;
    bool casesFromRequest = false;

    if (json.isMember("cases") && json["cases"].isArray()) {
        casesFromRequest = true;
        for (const auto& c : json["cases"]) {
            std::string input    = c.isMember("input")    ? c["input"].asString()    : "";
            std::string expected = c.isMember("expected") ? c["expected"].asString() : "";
            cases.push_back({input, expected, /*isCustom=*/false});
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
            cases.push_back({tc.getInput(), tc.getExpected(), /*isCustom=*/false});
        }
        delete problem;

        // Append any user-added custom cases (LeetCode-style).
        if (json.isMember("customCases") && json["customCases"].isArray()) {
            for (const auto& c : json["customCases"]) {
                std::string input = c.isMember("input") ? c["input"].asString() : "";
                // Drop empty rows — the user hasn't filled them in yet.
                if (input.empty()) continue;
                cases.push_back({input, /*expected=*/"", /*isCustom=*/true});
            }
        }
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
    int passed = 0;     // only counts DB cases (custom cases can't "pass")
    int total = 0;      // total cases run
    int dbTotal = 0;    // number of DB cases
    bool compileOk = true;
    std::string compileOut;
    bool stoppedEarly = false;

    for (size_t i = 0; i < cases.size(); ++i) {
        if (stoppedEarly) break;

        const RunCase& c = cases[i];
        if (!c.isCustom) dbTotal++;

        std::vector<std::pair<std::string, std::string>> singleCase = {
            {c.input, c.expected}
        };
        auto response = ExecutorService::getInstance().compileAndRun(code, singleCase);

        Json::Value caseObj;
        caseObj["position"]        = (int)i;
        caseObj["source"]          = c.isCustom ? "custom" : "db";
        caseObj["input"]           = c.input;
        // Custom cases have no expected — omit the field entirely so the
        // client can render them with no "expected" row at all.
        if (!c.isCustom) {
            caseObj["expected"] = c.expected;
        }
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
            // For DB cases this is a real AC (passed comparison).
            // For custom cases this is just "the code ran without errors" —
            // there's nothing to assert against, so we label it differently
            // so the client can render it as 完成 / OK.
            status = c.isCustom ? "OK" : "AC";
            if (!c.isCustom) passed++;
        }
        caseObj["status"] = status;
        casesArr.append(caseObj);
        total++;
    }

    // "passed" and "allPassed" only consider DB cases — custom cases have
    // no expected output to compare against, so they can't "pass" or "fail"
    // in the AC/WA sense.
    Json::Value result;
    result["compileSuccess"] = compileOk;
    result["compileOutput"]  = compileOut;
    result["cases"]          = casesArr;
    result["passed"]         = passed;
    result["total"]          = dbTotal;
    result["allPassed"]      = (dbTotal > 0 && passed == dbTotal);

    res.status = 200;
    res.set_content(Json::FastWriter().write(result), "application/json");
}

}
