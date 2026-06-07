#include <gtest/gtest.h>
#include <fstream>
#include <filesystem>
#include <json/json.h>
#include "run_handler.h"
#include "problem.h"
#include "test_case.h"
#include "executor_service.h"
#include "connection_pool.h"
#include "config.h"
#include "logger.h"
#include "../src/utils/httplib.h"

using namespace LogModule;

namespace {

class RunHandlerTest : public ::testing::Test {
protected:
    void SetUp() override {
        tempDir = std::filesystem::temp_directory_path() / "cpp_oj_run_handler_test";
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
    }

    void TearDown() override {
        std::filesystem::remove_all(tempDir);
    }

    std::filesystem::path tempDir;
    std::filesystem::path configFile;

    httplib::Request createRequest(const std::string& body) {
        httplib::Request req;
        req.method = "POST";
        req.path = "/api/run";
        req.body = body;
        return req;
    }

    Json::Value parseResponse(const httplib::Response& res) {
        Json::Value json;
        Json::CharReaderBuilder builder;
        std::istringstream iss(res.body);
        std::string err;
        Json::parseFromStream(builder, iss, &json, &err);
        return json;
    }

    int createProblemWithCases(const std::string& title,
                               const std::vector<std::pair<std::string, std::string>>& cases) {
        oj::Problem p;
        p.setTitle(title);
        p.setDifficulty("Easy");
        p.setContent("Run handler test");
        p.setTemplate("#include <iostream>");
        int pid = p.save();
        if (pid <= 0) return -1;

        int pos = 0;
        for (const auto& c : cases) {
            oj::TestCase tc;
            tc.setProblemId(pid);
            tc.setInput(c.first);
            tc.setExpected(c.second);
            tc.setPosition(pos++);
            tc.save();
        }
        return pid;
    }
};

// ---- Request validation ----------------------------------------------------

TEST_F(RunHandlerTest, InvalidJSON) {
    httplib::Request req = createRequest("not json");
    httplib::Response res;
    oj::RunHandler::runCode(req, res);

    EXPECT_EQ(res.status, 400);
    Json::Value body = parseResponse(res);
    EXPECT_EQ(body["error"], "Invalid JSON");
}

TEST_F(RunHandlerTest, MissingCodeField) {
    std::string body = R"({"problemId":1})";
    httplib::Request req = createRequest(body);
    httplib::Response res;
    oj::RunHandler::runCode(req, res);

    EXPECT_EQ(res.status, 400);
    Json::Value json = parseResponse(res);
    EXPECT_EQ(json["error"], "Missing required fields: code, problemId");
}

TEST_F(RunHandlerTest, MissingProblemIdField) {
    std::string body = R"({"code":"int main(){return 0;}"})";
    httplib::Request req = createRequest(body);
    httplib::Response res;
    oj::RunHandler::runCode(req, res);

    EXPECT_EQ(res.status, 400);
    Json::Value json = parseResponse(res);
    EXPECT_EQ(json["error"], "Missing required fields: code, problemId");
}

TEST_F(RunHandlerTest, MissingBothFields) {
    std::string body = R"({})";
    httplib::Request req = createRequest(body);
    httplib::Response res;
    oj::RunHandler::runCode(req, res);

    EXPECT_EQ(res.status, 400);
    Json::Value json = parseResponse(res);
    EXPECT_EQ(json["error"], "Missing required fields: code, problemId");
}

TEST_F(RunHandlerTest, EmptyCode) {
    int pid = createProblemWithCases("Empty Code Test", {{"1 2", "3\n"}});
    ASSERT_GT(pid, 0);

    Json::Value body;
    body["code"] = "";
    body["problemId"] = pid;
    httplib::Request req = createRequest(Json::FastWriter().write(body));
    httplib::Response res;
    oj::RunHandler::runCode(req, res);

    EXPECT_EQ(res.status, 400);
    Json::Value json = parseResponse(res);
    EXPECT_EQ(json["error"], "Code cannot be empty");

    auto p = oj::Problem::findById(pid);
    p->remove();
}

// ---- Problem / test case resolution ----------------------------------------

TEST_F(RunHandlerTest, ProblemNotFoundWhenNoCasesProvided) {
    Json::Value body;
    body["code"] = "#include <iostream>\nint main(){return 0;}";
    body["problemId"] = 999999;
    httplib::Request req = createRequest(Json::FastWriter().write(body));
    httplib::Response res;
    oj::RunHandler::runCode(req, res);

    EXPECT_EQ(res.status, 404);
    Json::Value json = parseResponse(res);
    EXPECT_EQ(json["error"], "Problem not found");
}

TEST_F(RunHandlerTest, ProblemNotFoundWithCustomCases) {
    std::string source = R"(#include <iostream>
int main() {
    int a, b;
    std::cin >> a >> b;
    std::cout << a + b << std::endl;
    return 0;
}
)";

    Json::Value body;
    body["code"] = source;
    body["problemId"] = 999999;
    Json::Value cases(Json::arrayValue);
    Json::Value c;
    c["input"] = "1 2";
    c["expected"] = "3\n";
    cases.append(c);
    body["cases"] = cases;
    httplib::Request req = createRequest(Json::FastWriter().write(body));
    httplib::Response res;
    oj::RunHandler::runCode(req, res);

    EXPECT_EQ(res.status, 200);
    Json::Value json = parseResponse(res);
    EXPECT_TRUE(json["compileSuccess"].asBool());
    EXPECT_EQ(json["passed"], 1);
    EXPECT_EQ(json["total"], 1);
    EXPECT_TRUE(json["allPassed"].asBool());
}

TEST_F(RunHandlerTest, NoTestCasesInDB) {
    oj::Problem p;
    p.setTitle("Empty Cases Run Test");
    p.setDifficulty("Easy");
    p.setContent("No cases");
    int pid = p.save();
    ASSERT_GT(pid, 0);

    Json::Value body;
    body["code"] = "#include <iostream>\nint main(){return 0;}";
    body["problemId"] = pid;
    httplib::Request req = createRequest(Json::FastWriter().write(body));
    httplib::Response res;
    oj::RunHandler::runCode(req, res);

    EXPECT_EQ(res.status, 400);
    Json::Value json = parseResponse(res);
    EXPECT_EQ(json["error"], "No test cases configured for this problem");

    auto prob = oj::Problem::findById(pid);
    prob->remove();
}

TEST_F(RunHandlerTest, EmptyCustomCasesArray) {
    int pid = createProblemWithCases("Ignored", {{"1 2", "3\n"}});
    ASSERT_GT(pid, 0);

    Json::Value body;
    body["code"] = "#include <iostream>\nint main(){return 0;}";
    body["problemId"] = pid;
    body["cases"] = Json::Value(Json::arrayValue);
    httplib::Request req = createRequest(Json::FastWriter().write(body));
    httplib::Response res;
    oj::RunHandler::runCode(req, res);

    EXPECT_EQ(res.status, 400);
    Json::Value json = parseResponse(res);
    EXPECT_EQ(json["error"], "请添加至少一个测试用例");

    auto p = oj::Problem::findById(pid);
    p->remove();
}

TEST_F(RunHandlerTest, CasesNotAnArrayFallsBackToDB) {
    int pid = createProblemWithCases("Fallback Test", {{"1 2", "3\n"}});
    ASSERT_GT(pid, 0);

    std::string source = R"(#include <iostream>
int main() {
    int a, b;
    std::cin >> a >> b;
    std::cout << a + b << std::endl;
    return 0;
}
)";

    Json::Value body;
    body["code"] = source;
    body["problemId"] = pid;
    body["cases"] = "not an array";
    httplib::Request req = createRequest(Json::FastWriter().write(body));
    httplib::Response res;
    oj::RunHandler::runCode(req, res);

    EXPECT_EQ(res.status, 200);
    Json::Value json = parseResponse(res);
    EXPECT_TRUE(json["compileSuccess"].asBool());
    EXPECT_EQ(json["passed"], 1);
    EXPECT_EQ(json["total"], 1);
    EXPECT_TRUE(json["allPassed"].asBool());

    auto p = oj::Problem::findById(pid);
    p->remove();
}

TEST_F(RunHandlerTest, CustomCaseMissingFieldsDefaultsToEmpty) {
    // The `cases` (legacy) path: user-supplied test cases, no DB lookup.
    // We verify that missing fields default to empty strings. With the new
    // empty-expected semantics, an empty expected means "no comparison"
    // (the code just needs to run without error); a non-empty expected
    // is still compared normally.
    std::string source = R"(#include <iostream>
int main() {
    int a, b;
    std::cin >> a >> b;
    std::cout << a + b << std::endl;
    return 0;
}
)";

    Json::Value body;
    body["code"] = source;
    body["problemId"] = 1;
    Json::Value cases(Json::arrayValue);
    {
        Json::Value c1;
        c1["input"] = "1 2";          // no expected
        cases.append(c1);
    }
    {
        Json::Value c2;
        c2["expected"] = "99";         // no input
        cases.append(c2);
    }
    body["cases"] = cases;
    httplib::Request req = createRequest(Json::FastWriter().write(body));
    httplib::Response res;
    oj::RunHandler::runCode(req, res);

    EXPECT_EQ(res.status, 200);
    Json::Value json = parseResponse(res);
    ASSERT_TRUE(json["cases"].isArray());
    ASSERT_EQ(json["cases"].size(), 2u);
    // c1: input="1 2", expected="" → no comparison → AC (runs fine)
    EXPECT_EQ(json["cases"][0]["status"], "AC");
    EXPECT_EQ(json["cases"][0]["input"].asString(), "1 2");
    EXPECT_EQ(json["cases"][0]["expected"].asString(), "");
    // c2: input="", expected="99" → comparison runs, actual won't equal
    // "99" so it's WA. (std::cin fails on empty input, leaving a/b
    // uninitialized, output is unpredictable but never exactly "99".)
    EXPECT_EQ(json["cases"][1]["status"], "WA");
}

// ---- Successful runs --------------------------------------------------------

TEST_F(RunHandlerTest, AllPassedWithDefaultCases) {
    int pid = createProblemWithCases("Default Cases Run",
        {{"1 2", "3\n"}, {"10 20", "30\n"}, {"-5 5", "0\n"}});
    ASSERT_GT(pid, 0);

    std::string source = R"(#include <iostream>
int main() {
    int a, b;
    std::cin >> a >> b;
    std::cout << a + b << std::endl;
    return 0;
}
)";

    Json::Value body;
    body["code"] = source;
    body["problemId"] = pid;
    httplib::Request req = createRequest(Json::FastWriter().write(body));
    httplib::Response res;
    oj::RunHandler::runCode(req, res);

    EXPECT_EQ(res.status, 200);
    Json::Value json = parseResponse(res);
    EXPECT_TRUE(json["compileSuccess"].asBool());
    EXPECT_EQ(json["passed"], 3);
    EXPECT_EQ(json["total"], 3);
    EXPECT_TRUE(json["allPassed"].asBool());
    ASSERT_TRUE(json["cases"].isArray());
    ASSERT_EQ(json["cases"].size(), 3u);
    for (const auto& c : json["cases"]) {
        EXPECT_EQ(c["status"], "AC");
        EXPECT_TRUE(c["compileSuccess"].asBool());
        // AC 分支也必须回填实际输出（修复后）
        EXPECT_FALSE(c["actual"].asString().empty());
    }

    auto p = oj::Problem::findById(pid);
    p->remove();
}

TEST_F(RunHandlerTest, AllPassedWithCustomCases) {
    std::string source = R"(#include <iostream>
int main() {
    int a, b;
    std::cin >> a >> b;
    std::cout << a + b << std::endl;
    return 0;
}
)";

    Json::Value body;
    body["code"] = source;
    body["problemId"] = 1;
    Json::Value cases(Json::arrayValue);
    {
        Json::Value c;
        c["input"] = "1 2";
        c["expected"] = "3\n";
        cases.append(c);
    }
    {
        Json::Value c;
        c["input"] = "5 7";
        c["expected"] = "12\n";
        cases.append(c);
    }
    body["cases"] = cases;

    httplib::Request req = createRequest(Json::FastWriter().write(body));
    httplib::Response res;
    oj::RunHandler::runCode(req, res);

    EXPECT_EQ(res.status, 200);
    Json::Value json = parseResponse(res);
    EXPECT_TRUE(json["compileSuccess"].asBool());
    EXPECT_EQ(json["passed"], 2);
    EXPECT_EQ(json["total"], 2);
    EXPECT_TRUE(json["allPassed"].asBool());
}

TEST_F(RunHandlerTest, PerCaseFieldsArePopulated) {
    int pid = createProblemWithCases("Per-case Fields", {{"1 2", "3\n"}});
    ASSERT_GT(pid, 0);

    std::string source = R"(#include <iostream>
int main() {
    int a, b;
    std::cin >> a >> b;
    std::cout << a + b << std::endl;
    return 0;
}
)";

    Json::Value body;
    body["code"] = source;
    body["problemId"] = pid;
    httplib::Request req = createRequest(Json::FastWriter().write(body));
    httplib::Response res;
    oj::RunHandler::runCode(req, res);

    ASSERT_EQ(res.status, 200);
    Json::Value json = parseResponse(res);
    ASSERT_TRUE(json["cases"].isArray());
    ASSERT_EQ(json["cases"].size(), 1u);

    const auto& c = json["cases"][0];
    EXPECT_EQ(c["position"].asInt(), 0);
    EXPECT_EQ(c["input"].asString(), "1 2");
    EXPECT_EQ(c["expected"].asString(), "3\n");
    EXPECT_EQ(c["actual"].asString(), "3\n");
    EXPECT_TRUE(c["compileSuccess"].asBool());
    EXPECT_EQ(c["status"], "AC");
    EXPECT_TRUE(c.isMember("executionTimeMs"));
    EXPECT_TRUE(c.isMember("compileOutput"));

    auto p = oj::Problem::findById(pid);
    p->remove();
}

TEST_F(RunHandlerTest, PositionFieldIncrements) {
    int pid = createProblemWithCases("Position Test",
        {{"1", "1\n"}, {"2", "2\n"}, {"3", "3\n"}});
    ASSERT_GT(pid, 0);

    std::string source = R"(#include <iostream>
int main() {
    int x;
    std::cin >> x;
    std::cout << x << std::endl;
    return 0;
}
)";

    Json::Value body;
    body["code"] = source;
    body["problemId"] = pid;
    httplib::Request req = createRequest(Json::FastWriter().write(body));
    httplib::Response res;
    oj::RunHandler::runCode(req, res);

    ASSERT_EQ(res.status, 200);
    Json::Value json = parseResponse(res);
    ASSERT_EQ(json["cases"].size(), 3u);
    EXPECT_EQ(json["cases"][0]["position"].asInt(), 0);
    EXPECT_EQ(json["cases"][1]["position"].asInt(), 1);
    EXPECT_EQ(json["cases"][2]["position"].asInt(), 2);

    auto p = oj::Problem::findById(pid);
    p->remove();
}

TEST_F(RunHandlerTest, MixedPassAndFail) {
    int pid = createProblemWithCases("Mixed Run",
        {{"1 2", "3\n"}, {"10 5", "5\n"}, {"4 4", "8\n"}});
    ASSERT_GT(pid, 0);

    std::string source = R"(#include <iostream>
int main() {
    int a, b;
    std::cin >> a >> b;
    std::cout << a + b << std::endl;
    return 0;
}
)";

    Json::Value body;
    body["code"] = source;
    body["problemId"] = pid;
    httplib::Request req = createRequest(Json::FastWriter().write(body));
    httplib::Response res;
    oj::RunHandler::runCode(req, res);

    EXPECT_EQ(res.status, 200);
    Json::Value json = parseResponse(res);
    EXPECT_EQ(json["passed"], 2);
    EXPECT_EQ(json["total"], 3);
    EXPECT_FALSE(json["allPassed"].asBool());
    EXPECT_EQ(json["cases"][0]["status"], "AC");
    EXPECT_EQ(json["cases"][1]["status"], "WA");
    EXPECT_EQ(json["cases"][2]["status"], "AC");

    auto p = oj::Problem::findById(pid);
    p->remove();
}

// ---- Compile / runtime error statuses ---------------------------------------

TEST_F(RunHandlerTest, CompileErrorMarksCE) {
    int pid = createProblemWithCases("CE Run", {{"1 2", "3\n"}, {"5 6", "11\n"}});
    ASSERT_GT(pid, 0);

    std::string source = R"(#include <iostream>
int main() {
    int x = undefined_variable;
    return 0;
}
)";

    Json::Value body;
    body["code"] = source;
    body["problemId"] = pid;
    httplib::Request req = createRequest(Json::FastWriter().write(body));
    httplib::Response res;
    oj::RunHandler::runCode(req, res);

    EXPECT_EQ(res.status, 200);
    Json::Value json = parseResponse(res);
    EXPECT_FALSE(json["compileSuccess"].asBool());
    EXPECT_FALSE(json["compileOutput"].asString().empty());
    EXPECT_FALSE(json["allPassed"].asBool());
    ASSERT_EQ(json["cases"].size(), 1u);
    EXPECT_EQ(json["cases"][0]["status"], "CE");
    EXPECT_FALSE(json["cases"][0]["compileSuccess"].asBool());

    auto p = oj::Problem::findById(pid);
    p->remove();
}

TEST_F(RunHandlerTest, CompileErrorStopsEarly) {
    int pid = createProblemWithCases("CE Stop Early",
        {{"1 2", "3\n"}, {"5 6", "11\n"}, {"7 8", "15\n"}});
    ASSERT_GT(pid, 0);

    std::string source = R"(int main() { int x = undefined_var; return 0; }
)";

    Json::Value body;
    body["code"] = source;
    body["problemId"] = pid;
    httplib::Request req = createRequest(Json::FastWriter().write(body));
    httplib::Response res;
    oj::RunHandler::runCode(req, res);

    EXPECT_EQ(res.status, 200);
    Json::Value json = parseResponse(res);
    EXPECT_FALSE(json["compileSuccess"].asBool());
    EXPECT_FALSE(json["allPassed"].asBool());
    EXPECT_EQ(json["total"], 1);
    EXPECT_EQ(json["passed"], 0);

    auto p = oj::Problem::findById(pid);
    p->remove();
}

TEST_F(RunHandlerTest, RuntimeErrorMarksRE) {
    int pid = createProblemWithCases("RE Run", {{"", ""}});
    ASSERT_GT(pid, 0);

    std::string source = R"(#include <iostream>
int main() {
    int* p = nullptr;
    *p = 42;
    return 0;
}
)";

    Json::Value body;
    body["code"] = source;
    body["problemId"] = pid;
    httplib::Request req = createRequest(Json::FastWriter().write(body));
    httplib::Response res;
    oj::RunHandler::runCode(req, res);

    EXPECT_EQ(res.status, 200);
    Json::Value json = parseResponse(res);
    EXPECT_TRUE(json["compileSuccess"].asBool());
    EXPECT_FALSE(json["allPassed"].asBool());
    ASSERT_EQ(json["cases"].size(), 1u);
    EXPECT_EQ(json["cases"][0]["status"], "RE");

    auto p = oj::Problem::findById(pid);
    p->remove();
}

TEST_F(RunHandlerTest, TimeoutMarksTLE) {
    int pid = createProblemWithCases("TLE Run", {{"", ""}});
    ASSERT_GT(pid, 0);

    std::string source = R"(#include <iostream>
#include <thread>
#include <chrono>
int main() {
    std::this_thread::sleep_for(std::chrono::seconds(10));
    return 0;
}
)";

    Json::Value body;
    body["code"] = source;
    body["problemId"] = pid;
    httplib::Request req = createRequest(Json::FastWriter().write(body));
    httplib::Response res;
    oj::RunHandler::runCode(req, res);

    EXPECT_EQ(res.status, 200);
    Json::Value json = parseResponse(res);
    EXPECT_TRUE(json["compileSuccess"].asBool());
    ASSERT_EQ(json["cases"].size(), 1u);
    EXPECT_EQ(json["cases"][0]["status"], "TLE");

    auto p = oj::Problem::findById(pid);
    p->remove();
}

TEST_F(RunHandlerTest, WrongAnswerMarksWA) {
    int pid = createProblemWithCases("WA Run", {{"1 2", "3\n"}});
    ASSERT_GT(pid, 0);

    std::string source = R"(#include <iostream>
int main() {
    int a, b;
    std::cin >> a >> b;
    std::cout << a - b << std::endl;
    return 0;
}
)";

    Json::Value body;
    body["code"] = source;
    body["problemId"] = pid;
    httplib::Request req = createRequest(Json::FastWriter().write(body));
    httplib::Response res;
    oj::RunHandler::runCode(req, res);

    EXPECT_EQ(res.status, 200);
    Json::Value json = parseResponse(res);
    EXPECT_TRUE(json["compileSuccess"].asBool());
    EXPECT_FALSE(json["allPassed"].asBool());
    ASSERT_EQ(json["cases"].size(), 1u);
    EXPECT_EQ(json["cases"][0]["status"], "WA");

    auto p = oj::Problem::findById(pid);
    p->remove();
}

// ---- Response shape ---------------------------------------------------------

TEST_F(RunHandlerTest, ResponseContainsExpectedTopLevelFields) {
    int pid = createProblemWithCases("Shape Run", {{"1 2", "3\n"}});
    ASSERT_GT(pid, 0);

    std::string source = R"(#include <iostream>
int main() {
    int a, b;
    std::cin >> a >> b;
    std::cout << a + b << std::endl;
    return 0;
}
)";

    Json::Value body;
    body["code"] = source;
    body["problemId"] = pid;
    httplib::Request req = createRequest(Json::FastWriter().write(body));
    httplib::Response res;
    oj::RunHandler::runCode(req, res);

    ASSERT_EQ(res.status, 200);
    Json::Value json = parseResponse(res);
    EXPECT_TRUE(json.isMember("compileSuccess"));
    EXPECT_TRUE(json.isMember("compileOutput"));
    EXPECT_TRUE(json.isMember("cases"));
    EXPECT_TRUE(json.isMember("passed"));
    EXPECT_TRUE(json.isMember("total"));
    EXPECT_TRUE(json.isMember("allPassed"));

    auto p = oj::Problem::findById(pid);
    p->remove();
}

TEST_F(RunHandlerTest, AllCasesRunEvenAfterWA) {
    int pid = createProblemWithCases("Continue After WA",
        {{"1 2", "3\n"}, {"5 6", "11\n"}});
    ASSERT_GT(pid, 0);

    std::string source = R"(#include <iostream>
int main() {
    int a, b;
    std::cin >> a >> b;
    std::cout << a - b << std::endl;
    return 0;
}
)";

    Json::Value body;
    body["code"] = source;
    body["problemId"] = pid;
    httplib::Request req = createRequest(Json::FastWriter().write(body));
    httplib::Response res;
    oj::RunHandler::runCode(req, res);

    EXPECT_EQ(res.status, 200);
    Json::Value json = parseResponse(res);
    EXPECT_EQ(json["total"], 2);
    EXPECT_EQ(json["passed"], 0);
    EXPECT_FALSE(json["allPassed"].asBool());
    EXPECT_EQ(json["cases"][0]["status"], "WA");
    EXPECT_EQ(json["cases"][1]["status"], "WA");

    auto p = oj::Problem::findById(pid);
    p->remove();
}

// ---- customCases (LeetCode-style) -------------------------------------------
//
// New API: client sends only the user-added custom cases (input only, no
// expected). The server loads the problem's official DB cases and appends
// them. Each result case is tagged with `source: "db" | "custom"`.

TEST_F(RunHandlerTest, CustomCasesMergedWithDB) {
    // Three official cases + two user-added custom cases = 5 total results.
    int pid = createProblemWithCases("Merged Run",
        {{"1 2", "3\n"}, {"10 20", "30\n"}, {"-5 5", "0\n"}});
    ASSERT_GT(pid, 0);

    std::string source = R"(#include <iostream>
int main() {
    int a, b;
    std::cin >> a >> b;
    std::cout << a + b << std::endl;
    return 0;
}
)";

    Json::Value body;
    body["code"] = source;
    body["problemId"] = pid;
    Json::Value customCases(Json::arrayValue);
    {
        Json::Value c;
        c["input"] = "100 200";
        customCases.append(c);
    }
    {
        Json::Value c;
        c["input"] = "7 8";
        customCases.append(c);
    }
    body["customCases"] = customCases;
    httplib::Request req = createRequest(Json::FastWriter().write(body));
    httplib::Response res;
    oj::RunHandler::runCode(req, res);

    EXPECT_EQ(res.status, 200);
    Json::Value json = parseResponse(res);
    EXPECT_TRUE(json["compileSuccess"].asBool());
    // passed/total only count DB cases
    EXPECT_EQ(json["passed"], 3);
    EXPECT_EQ(json["total"], 3);
    EXPECT_TRUE(json["allPassed"].asBool());

    ASSERT_EQ(json["cases"].size(), 5u);
    // First three are DB (official), last two are custom
    for (int i = 0; i < 3; ++i) {
        EXPECT_EQ(json["cases"][i]["source"].asString(), "db");
        EXPECT_EQ(json["cases"][i]["status"].asString(), "AC");
    }
    for (int i = 3; i < 5; ++i) {
        EXPECT_EQ(json["cases"][i]["source"].asString(), "custom");
        // Custom cases: no comparison, just "ran without error" → "OK"
        EXPECT_EQ(json["cases"][i]["status"].asString(), "OK");
        // Custom cases must NOT have an "expected" field
        EXPECT_FALSE(json["cases"][i].isMember("expected"));
    }
    // Position is global (0..4)
    EXPECT_EQ(json["cases"][0]["position"].asInt(), 0);
    EXPECT_EQ(json["cases"][3]["position"].asInt(), 3);
    EXPECT_EQ(json["cases"][4]["position"].asInt(), 4);

    auto p = oj::Problem::findById(pid);
    p->remove();
}

TEST_F(RunHandlerTest, CustomCasesDontAffectPassedCount) {
    // All DB cases WA, all custom cases OK — passed should be 0, allPassed
    // should be false (DB cases failed), even though custom cases ran fine.
    int pid = createProblemWithCases("CustomNoEffect",
        {{"1 2", "3\n"}, {"5 6", "11\n"}});
    ASSERT_GT(pid, 0);

    std::string source = R"(#include <iostream>
int main() {
    int a, b;
    std::cin >> a >> b;
    std::cout << a - b << std::endl;   // intentionally wrong
    return 0;
}
)";

    Json::Value body;
    body["code"] = source;
    body["problemId"] = pid;
    Json::Value customCases(Json::arrayValue);
    {
        Json::Value c;
        c["input"] = "100 200";
        customCases.append(c);
    }
    body["customCases"] = customCases;
    httplib::Request req = createRequest(Json::FastWriter().write(body));
    httplib::Response res;
    oj::RunHandler::runCode(req, res);

    EXPECT_EQ(res.status, 200);
    Json::Value json = parseResponse(res);
    EXPECT_EQ(json["passed"], 0);
    EXPECT_EQ(json["total"], 2);   // only DB cases counted
    EXPECT_FALSE(json["allPassed"].asBool());
    ASSERT_EQ(json["cases"].size(), 3u);
    EXPECT_EQ(json["cases"][0]["status"].asString(), "WA");
    EXPECT_EQ(json["cases"][1]["status"].asString(), "WA");
    EXPECT_EQ(json["cases"][2]["status"].asString(), "OK");
    EXPECT_EQ(json["cases"][2]["source"].asString(), "custom");

    auto p = oj::Problem::findById(pid);
    p->remove();
}

TEST_F(RunHandlerTest, CustomCasesWithEmptyInputDropped) {
    // Empty custom cases are filtered out — they're placeholders the user
    // hasn't filled in yet. They should not count toward execution.
    int pid = createProblemWithCases("DropEmptyCustom",
        {{"1 2", "3\n"}});
    ASSERT_GT(pid, 0);

    std::string source = R"(#include <iostream>
int main() {
    int a, b;
    std::cin >> a >> b;
    std::cout << a + b << std::endl;
    return 0;
}
)";

    Json::Value body;
    body["code"] = source;
    body["problemId"] = pid;
    Json::Value customCases(Json::arrayValue);
    {
        Json::Value c;
        c["input"] = "";   // empty — should be dropped
        customCases.append(c);
    }
    {
        Json::Value c;
        c["input"] = "5 6";
        customCases.append(c);
    }
    body["customCases"] = customCases;
    httplib::Request req = createRequest(Json::FastWriter().write(body));
    httplib::Response res;
    oj::RunHandler::runCode(req, res);

    EXPECT_EQ(res.status, 200);
    Json::Value json = parseResponse(res);
    // Only the 1 DB case + 1 non-empty custom case = 2 results
    ASSERT_EQ(json["cases"].size(), 2u);
    EXPECT_EQ(json["cases"][0]["source"].asString(), "db");
    EXPECT_EQ(json["cases"][1]["source"].asString(), "custom");

    auto p = oj::Problem::findById(pid);
    p->remove();
}

TEST_F(RunHandlerTest, NoCustomCasesStillRunsDB) {
    // If the user has no custom cases, the request just runs the DB cases.
    // (The "add case" placeholder is filtered out as empty input.)
    int pid = createProblemWithCases("JustDB",
        {{"1 2", "3\n"}, {"5 6", "11\n"}});
    ASSERT_GT(pid, 0);

    std::string source = R"(#include <iostream>
int main() {
    int a, b;
    std::cin >> a >> b;
    std::cout << a + b << std::endl;
    return 0;
}
)";

    Json::Value body;
    body["code"] = source;
    body["problemId"] = pid;
    body["customCases"] = Json::Value(Json::arrayValue);
    httplib::Request req = createRequest(Json::FastWriter().write(body));
    httplib::Response res;
    oj::RunHandler::runCode(req, res);

    EXPECT_EQ(res.status, 200);
    Json::Value json = parseResponse(res);
    EXPECT_EQ(json["passed"], 2);
    EXPECT_EQ(json["total"], 2);
    EXPECT_TRUE(json["allPassed"].asBool());
    ASSERT_EQ(json["cases"].size(), 2u);
    for (const auto& c : json["cases"]) {
        EXPECT_EQ(c["source"].asString(), "db");
        EXPECT_EQ(c["status"].asString(), "AC");
    }

    auto p = oj::Problem::findById(pid);
    p->remove();
}

TEST_F(RunHandlerTest, CustomCaseRuntimeErrorMarksRE) {
    // A custom case that crashes (RE) should still be reported with the
    // proper error status — we just don't compare against expected.
    int pid = createProblemWithCases("CustomRE", {{"1 2", "3\n"}});
    ASSERT_GT(pid, 0);

    std::string source = R"(#include <iostream>
int main() {
    int* p = nullptr;
    *p = 42;
    return 0;
}
)";

    Json::Value body;
    body["code"] = source;
    body["problemId"] = pid;
    Json::Value customCases(Json::arrayValue);
    {
        Json::Value c;
        c["input"] = "1 2";
        customCases.append(c);
    }
    body["customCases"] = customCases;
    httplib::Request req = createRequest(Json::FastWriter().write(body));
    httplib::Response res;
    oj::RunHandler::runCode(req, res);

    EXPECT_EQ(res.status, 200);
    Json::Value json = parseResponse(res);
    ASSERT_EQ(json["cases"].size(), 2u);
    EXPECT_EQ(json["cases"][0]["source"].asString(), "db");
    EXPECT_EQ(json["cases"][0]["status"].asString(), "RE");
    EXPECT_EQ(json["cases"][1]["source"].asString(), "custom");
    EXPECT_EQ(json["cases"][1]["status"].asString(), "RE");

    auto p = oj::Problem::findById(pid);
    p->remove();
}

TEST_F(RunHandlerTest, LegacyCasesOverridesDB) {
    // The legacy `cases` field (user-provided, with expected) should still
    // REPLACE the DB cases — it does NOT merge with them. This preserves
    // backward compat with the old API used by the integration tests.
    int pid = createProblemWithCases("LegacyOverride",
        {{"1 2", "3\n"}, {"99 99", "198\n"}});
    ASSERT_GT(pid, 0);

    std::string source = R"(#include <iostream>
int main() {
    int a, b;
    std::cin >> a >> b;
    std::cout << a + b << std::endl;
    return 0;
}
)";

    Json::Value body;
    body["code"] = source;
    body["problemId"] = pid;
    Json::Value cases(Json::arrayValue);
    {
        Json::Value c;
        c["input"] = "10 20";
        c["expected"] = "30\n";
        cases.append(c);
    }
    body["cases"] = cases;
    httplib::Request req = createRequest(Json::FastWriter().write(body));
    httplib::Response res;
    oj::RunHandler::runCode(req, res);

    EXPECT_EQ(res.status, 200);
    Json::Value json = parseResponse(res);
    // Only the legacy case runs (DB cases are NOT merged in)
    ASSERT_EQ(json["cases"].size(), 1u);
    EXPECT_EQ(json["cases"][0]["status"].asString(), "AC");

    auto p = oj::Problem::findById(pid);
    p->remove();
}

} // namespace
