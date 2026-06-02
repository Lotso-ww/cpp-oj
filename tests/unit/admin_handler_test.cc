#include <gtest/gtest.h>
#include <fstream>
#include <filesystem>
#include <thread>
#include <atomic>
#include <json/json.h>
#include "admin_handler.h"
#include "problem.h"
#include "test_case.h"
#include "user.h"
#include "connection_pool.h"
#include "config.h"
#include "logger.h"
#include "../src/utils/httplib.h"

using namespace LogModule;

namespace {

class AdminHandlerTest : public ::testing::Test {
protected:
    void SetUp() override {
        tempDir = std::filesystem::temp_directory_path() / "cpp_oj_admin_handler_test";
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

    httplib::Request createRequest(const std::string& method, const std::string& path,
                                   const std::string& body = "") {
        httplib::Request req;
        req.method = method;
        req.path = path;
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

    int countProblemsInDB() {
        auto problems = oj::Problem::findAll();
        return static_cast<int>(problems.size());
    }

    int createTestProblem(const std::string& title = "DB Test Problem") {
        Json::Value body;
        body["title"] = title;
        body["difficulty"] = "Easy";
        body["content"] = "Test content for DB verification";
        std::string bodyStr = Json::FastWriter().write(body);
        httplib::Request req = createRequest("POST", "/api/admin/problems", bodyStr);
        httplib::Response res;
        oj::AdminHandler::createProblem(req, res);
        Json::Value response = parseResponse(res);
        return response["id"].asInt();
    }
};

TEST_F(AdminHandlerTest, CreateProblemInvalidJSON) {
    httplib::Request req = createRequest("POST", "/api/admin/problems", "not json");
    httplib::Response res;

    oj::AdminHandler::createProblem(req, res);

    EXPECT_EQ(res.status, 400);
    Json::Value error = parseResponse(res);
    EXPECT_EQ(error["error"], "Invalid JSON");
}

TEST_F(AdminHandlerTest, CreateProblemMissingRequiredFields) {
    std::string body = "{\"title\":\"Test\"}";
    httplib::Request req = createRequest("POST", "/api/admin/problems", body);
    httplib::Response res;

    oj::AdminHandler::createProblem(req, res);

    EXPECT_EQ(res.status, 400);
    Json::Value error = parseResponse(res);
    EXPECT_EQ(error["error"], "Missing required fields: title, difficulty, content");
}

TEST_F(AdminHandlerTest, CreateProblemEmptyFields) {
    std::string body = "{\"title\":\"\",\"difficulty\":\"Easy\",\"content\":\"Test content\"}";
    httplib::Request req = createRequest("POST", "/api/admin/problems", body);
    httplib::Response res;

    oj::AdminHandler::createProblem(req, res);

    EXPECT_EQ(res.status, 400);
}

TEST_F(AdminHandlerTest, CreateProblemInvalidDifficulty) {
    std::string body = "{\"title\":\"Test\",\"difficulty\":\"Invalid\",\"content\":\"Test content\"}";
    httplib::Request req = createRequest("POST", "/api/admin/problems", body);
    httplib::Response res;

    oj::AdminHandler::createProblem(req, res);

    EXPECT_EQ(res.status, 400);
    Json::Value error = parseResponse(res);
    EXPECT_EQ(error["error"], "Invalid difficulty. Must be Easy, Medium, or Hard");
}

TEST_F(AdminHandlerTest, CreateProblemSuccessEasy) {
    std::string body = R"({"title":"Two Sum","difficulty":"Easy","content":"Find two numbers"})";
    httplib::Request req = createRequest("POST", "/api/admin/problems", body);
    httplib::Response res;

    oj::AdminHandler::createProblem(req, res);

    EXPECT_EQ(res.status, 201);
    Json::Value response = parseResponse(res);
    ASSERT_TRUE(response.isMember("id"));
    int problemId = response["id"].asInt();
    ASSERT_GT(problemId, 0);

    auto p = oj::Problem::findById(problemId);
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(p->getTitle(), "Two Sum");
    EXPECT_EQ(p->getDifficulty(), "Easy");
    EXPECT_EQ(p->getContent(), "Find two numbers");
    delete p;
    p = oj::Problem::findById(problemId);
    p->remove();
}

TEST_F(AdminHandlerTest, CreateProblemSuccessMedium) {
    std::string body = R"({"title":"Median Sort","difficulty":"Medium","content":"Find median"})";
    httplib::Request req = createRequest("POST", "/api/admin/problems", body);
    httplib::Response res;

    oj::AdminHandler::createProblem(req, res);

    EXPECT_EQ(res.status, 201);
    Json::Value response = parseResponse(res);
    int problemId = response["id"].asInt();

    auto p = oj::Problem::findById(problemId);
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(p->getDifficulty(), "Medium");
    delete p;
    p = oj::Problem::findById(problemId);
    p->remove();
}

TEST_F(AdminHandlerTest, CreateProblemSuccessHard) {
    std::string body = R"({"title":"Hard Problem","difficulty":"Hard","content":"Complex task"})";
    httplib::Request req = createRequest("POST", "/api/admin/problems", body);
    httplib::Response res;

    oj::AdminHandler::createProblem(req, res);

    EXPECT_EQ(res.status, 201);
    Json::Value response = parseResponse(res);
    int problemId = response["id"].asInt();

    auto p = oj::Problem::findById(problemId);
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(p->getDifficulty(), "Hard");
    delete p;
    p = oj::Problem::findById(problemId);
    p->remove();
}

TEST_F(AdminHandlerTest, CreateProblemWithTemplate) {
    std::string body = R"({"title":"With Template","difficulty":"Easy","content":"Test","template":"#include <iostream>"})";
    httplib::Request req = createRequest("POST", "/api/admin/problems", body);
    httplib::Response res;

    oj::AdminHandler::createProblem(req, res);

    EXPECT_EQ(res.status, 201);
    Json::Value response = parseResponse(res);
    int problemId = response["id"].asInt();

    auto p = oj::Problem::findById(problemId);
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(p->getTemplate(), "#include <iostream>");
    delete p;
    p = oj::Problem::findById(problemId);
    p->remove();
}

TEST_F(AdminHandlerTest, CreateProblemWithTestCases) {
    std::string body = R"({
        "title":"With TestCases",
        "difficulty":"Easy",
        "content":"Test",
        "testCases":[
            {"input":"1 2","expected":"3"},
            {"input":"5 6","expected":"11"}
        ]
    })";
    httplib::Request req = createRequest("POST", "/api/admin/problems", body);
    httplib::Response res;

    oj::AdminHandler::createProblem(req, res);

    EXPECT_EQ(res.status, 201);
    Json::Value response = parseResponse(res);
    int problemId = response["id"].asInt();

    auto p = oj::Problem::findById(problemId);
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(p->getTestCases().size(), 2);
    EXPECT_EQ(p->getTestCases()[0].getInput(), "1 2");
    EXPECT_EQ(p->getTestCases()[0].getExpected(), "3");
    delete p;
    p = oj::Problem::findById(problemId);
    p->remove();
}

TEST_F(AdminHandlerTest, CreateProblemWithTestCasesPartial) {
    std::string body = R"({
        "title":"Partial TestCases",
        "difficulty":"Easy",
        "content":"Test",
        "testCases":[
            {"input":"1 2"},
            {"expected":"11"}
        ]
    })";
    httplib::Request req = createRequest("POST", "/api/admin/problems", body);
    httplib::Response res;

    oj::AdminHandler::createProblem(req, res);

    EXPECT_EQ(res.status, 201);
    Json::Value response = parseResponse(res);
    int problemId = response["id"].asInt();

    auto p = oj::Problem::findById(problemId);
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(p->getTestCases().size(), 0);
    delete p;
    p = oj::Problem::findById(problemId);
    p->remove();
}

TEST_F(AdminHandlerTest, DeleteProblemMissingId) {
    httplib::Request req = createRequest("DELETE", "/api/admin/problems/");
    httplib::Response res;

    oj::AdminHandler::deleteProblem(req, res);

    EXPECT_EQ(res.status, 400);
    Json::Value error = parseResponse(res);
    EXPECT_EQ(error["error"], "Invalid problem ID");
}

TEST_F(AdminHandlerTest, DeleteProblemInvalidId) {
    httplib::Request req = createRequest("DELETE", "/api/admin/problems/abc");
    req.path_params["id"] = "abc";
    httplib::Response res;

    oj::AdminHandler::deleteProblem(req, res);

    EXPECT_EQ(res.status, 400);
    Json::Value error = parseResponse(res);
    EXPECT_EQ(error["error"], "Invalid problem ID");
}

TEST_F(AdminHandlerTest, DeleteProblemNegativeId) {
    httplib::Request req = createRequest("DELETE", "/api/admin/problems/-1");
    req.path_params["id"] = "-1";
    httplib::Response res;

    oj::AdminHandler::deleteProblem(req, res);

    EXPECT_EQ(res.status, 400);
    Json::Value error = parseResponse(res);
    EXPECT_EQ(error["error"], "Invalid problem ID");
}

TEST_F(AdminHandlerTest, DeleteProblemZeroId) {
    httplib::Request req = createRequest("DELETE", "/api/admin/problems/0");
    req.path_params["id"] = "0";
    httplib::Response res;

    oj::AdminHandler::deleteProblem(req, res);

    EXPECT_EQ(res.status, 400);
    Json::Value error = parseResponse(res);
    EXPECT_EQ(error["error"], "Invalid problem ID");
}

TEST_F(AdminHandlerTest, DeleteProblemNotFound) {
    httplib::Request req = createRequest("DELETE", "/api/admin/problems/999999");
    req.path_params["id"] = "999999";
    httplib::Response res;

    oj::AdminHandler::deleteProblem(req, res);

    EXPECT_EQ(res.status, 404);
    Json::Value error = parseResponse(res);
    EXPECT_EQ(error["error"], "Problem not found");
}

TEST_F(AdminHandlerTest, DeleteProblemSuccess) {
    oj::Problem p;
    p.setTitle("To Be Deleted");
    p.setDifficulty("Easy");
    p.setContent("content");
    int pid = p.save();
    ASSERT_GT(pid, 0);

    httplib::Request req = createRequest("DELETE", "/api/admin/problems/" + std::to_string(pid));
    req.path_params["id"] = std::to_string(pid);
    httplib::Response res;

    oj::AdminHandler::deleteProblem(req, res);

    EXPECT_EQ(res.status, 200);
    Json::Value response = parseResponse(res);
    EXPECT_EQ(response["message"], "Problem deleted successfully");

    auto found = oj::Problem::findById(pid);
    EXPECT_EQ(found, nullptr);
}

TEST_F(AdminHandlerTest, DeleteProblemCascadesTestCases) {
    oj::Problem p;
    p.setTitle("Cascade Test");
    p.setDifficulty("Medium");
    p.setContent("content");
    int pid = p.save();
    ASSERT_GT(pid, 0);

    oj::TestCase tc;
    tc.setProblemId(pid);
    tc.setInput("1 2");
    tc.setExpected("3");
    tc.setPosition(0);
    int tcId = tc.save();
    ASSERT_GT(tcId, 0);

    httplib::Request req = createRequest("DELETE", "/api/admin/problems/" + std::to_string(pid));
    req.path_params["id"] = std::to_string(pid);
    httplib::Response res;

    oj::AdminHandler::deleteProblem(req, res);

    EXPECT_EQ(res.status, 200);

    auto cases = oj::TestCase::findByProblemId(pid);
    EXPECT_EQ(cases.size(), 0);
}

TEST_F(AdminHandlerTest, DBCreateProblemInsertsIntoProblemsTable) {
    int beforeCount = countProblemsInDB();

    std::string body = R"({"title":"DB Insert Test","difficulty":"Medium","content":"Verify DB insert"})";
    httplib::Request req = createRequest("POST", "/api/admin/problems", body);
    httplib::Response res;
    oj::AdminHandler::createProblem(req, res);

    ASSERT_EQ(res.status, 201);
    Json::Value response = parseResponse(res);
    int problemId = response["id"].asInt();

    int afterCount = countProblemsInDB();
    EXPECT_EQ(afterCount, beforeCount + 1);

    auto p = oj::Problem::findById(problemId);
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(p->getTitle(), "DB Insert Test");
    EXPECT_EQ(p->getDifficulty(), "Medium");
    delete p;
    p = oj::Problem::findById(problemId);
    p->remove();

    int finalCount = countProblemsInDB();
    EXPECT_EQ(finalCount, beforeCount);
}

TEST_F(AdminHandlerTest, DBCreateProblemWithTestCasesInsertsIntoTestCasesTable) {
    int problemId = createTestProblem("TC DB Test");

    auto p = oj::Problem::findById(problemId);
    ASSERT_NE(p, nullptr);
    int beforeTC = static_cast<int>(p->getTestCases().size());
    delete p;

    Json::Value body(Json::objectValue);
    body["title"] = "TC DB Test 2";
    body["difficulty"] = "Easy";
    body["content"] = "content";
    body["testCases"] = Json::Value(Json::arrayValue);
    body["testCases"].append(Json::Value());
    body["testCases"][0]["input"] = "a b";
    body["testCases"][0]["expected"] = "ab";
    body["testCases"].append(Json::Value());
    body["testCases"][1]["input"] = "x y";
    body["testCases"][1]["expected"] = "xy";
    body["testCases"].append(Json::Value());
    body["testCases"][2]["input"] = "p q";
    body["testCases"][2]["expected"] = "pq";
    std::string bodyStr = Json::FastWriter().write(body);
    httplib::Request req = createRequest("POST", "/api/admin/problems", bodyStr);
    httplib::Response res;
    oj::AdminHandler::createProblem(req, res);

    ASSERT_EQ(res.status, 201);
    Json::Value response = parseResponse(res);
    int newProblemId = response["id"].asInt();

    p = oj::Problem::findById(newProblemId);
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(p->getTestCases().size(), 3);
    EXPECT_EQ(p->getTestCases()[0].getInput(), "a b");
    EXPECT_EQ(p->getTestCases()[1].getExpected(), "xy");
    delete p;
    p = oj::Problem::findById(newProblemId);
    p->remove();
}

TEST_F(AdminHandlerTest, DBDeleteProblemRemovesFromProblemsTable) {
    int problemId = createTestProblem("Delete DB Test");
    int beforeCount = countProblemsInDB();

    httplib::Request req = createRequest("DELETE", "/api/admin/problems/" + std::to_string(problemId));
    req.path_params["id"] = std::to_string(problemId);
    httplib::Response res;
    oj::AdminHandler::deleteProblem(req, res);

    EXPECT_EQ(res.status, 200);
    int afterCount = countProblemsInDB();
    EXPECT_EQ(afterCount, beforeCount - 1);

    auto found = oj::Problem::findById(problemId);
    EXPECT_EQ(found, nullptr);
}

TEST_F(AdminHandlerTest, DBDeleteProblemCascadesTestCasesFromTestCasesTable) {
    int problemId = createTestProblem("Cascade DB Test");

    Json::Value tcBody(Json::objectValue);
    tcBody["title"] = "TC Cascade Test";
    tcBody["difficulty"] = "Easy";
    tcBody["content"] = "content";
    tcBody["testCases"] = Json::Value(Json::arrayValue);
    tcBody["testCases"].append(Json::Value());
    tcBody["testCases"][0]["input"] = "1";
    tcBody["testCases"][0]["expected"] = "2";
    tcBody["testCases"].append(Json::Value());
    tcBody["testCases"][1]["input"] = "3";
    tcBody["testCases"][1]["expected"] = "4";
    std::string tcBodyStr = Json::FastWriter().write(tcBody);
    httplib::Request tcReq = createRequest("POST", "/api/admin/problems", tcBodyStr);
    httplib::Response tcRes;
    oj::AdminHandler::createProblem(tcReq, tcRes);
    Json::Value tcResponse = parseResponse(tcRes);
    int tcProblemId = tcResponse["id"].asInt();

    auto p = oj::Problem::findById(tcProblemId);
    ASSERT_NE(p, nullptr);
    ASSERT_EQ(p->getTestCases().size(), 2);
    delete p;

    httplib::Request req = createRequest("DELETE", "/api/admin/problems/" + std::to_string(tcProblemId));
    req.path_params["id"] = std::to_string(tcProblemId);
    httplib::Response res;
    oj::AdminHandler::deleteProblem(req, res);

    EXPECT_EQ(res.status, 200);

    auto cases = oj::TestCase::findByProblemId(tcProblemId);
    EXPECT_EQ(cases.size(), 0);

    oj::Problem::findById(problemId)->remove();
}

TEST_F(AdminHandlerTest, DBProblemSaveReturnsValidIdAndCanBeFound) {
    std::string body = R"({"title":"Find By ID DB","difficulty":"Hard","content":"DB find test"})";
    httplib::Request req = createRequest("POST", "/api/admin/problems", body);
    httplib::Response res;
    oj::AdminHandler::createProblem(req, res);

    ASSERT_EQ(res.status, 201);
    Json::Value response = parseResponse(res);
    int problemId = response["id"].asInt();
    ASSERT_GT(problemId, 0);

    auto p = oj::Problem::findById(problemId);
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(p->getId(), problemId);
    EXPECT_EQ(p->getTitle(), "Find By ID DB");
    delete p;

    p = oj::Problem::findById(problemId);
    p->remove();
}

TEST_F(AdminHandlerTest, DBCreateProblemPersistsAllFields) {
    Json::Value body(Json::objectValue);
    body["title"] = "Persist Fields Test";
    body["difficulty"] = "Medium";
    body["content"] = "Full persistence test content";
    body["template"] = "#include <bits/stdc++.h>";
    std::string bodyStr = Json::FastWriter().write(body);
    httplib::Request req = createRequest("POST", "/api/admin/problems", bodyStr);
    httplib::Response res;
    oj::AdminHandler::createProblem(req, res);

    ASSERT_EQ(res.status, 201);
    Json::Value response = parseResponse(res);
    int problemId = response["id"].asInt();

    auto conn = oj::getConnection();
    ASSERT_NE(conn, nullptr);

    std::string query = "SELECT title, difficulty, content, template FROM problems WHERE id = " + std::to_string(problemId);
    mysql_query(conn, query.c_str());
    MYSQL_RES* result = mysql_store_result(conn);
    ASSERT_NE(result, nullptr);

    MYSQL_ROW row = mysql_fetch_row(result);
    ASSERT_NE(row, nullptr);
    EXPECT_STREQ(row[0], "Persist Fields Test");
    EXPECT_STREQ(row[1], "Medium");
    EXPECT_STREQ(row[2], "Full persistence test content");
    EXPECT_STREQ(row[3], "#include <bits/stdc++.h>");

    mysql_free_result(result);
    oj::releaseConnection(conn);

    auto p = oj::Problem::findById(problemId);
    p->remove();
}

TEST_F(AdminHandlerTest, DBDeleteNonExistentProblemDoesNotAffectOtherProblems) {
    int id1 = createTestProblem("Other Problem 1");
    int id2 = createTestProblem("Other Problem 2");
    int beforeCount = countProblemsInDB();

    httplib::Request req = createRequest("DELETE", "/api/admin/problems/999999");
    req.path_params["id"] = "999999";
    httplib::Response res;
    oj::AdminHandler::deleteProblem(req, res);

    EXPECT_EQ(res.status, 404);
    EXPECT_EQ(countProblemsInDB(), beforeCount);

    auto p1 = oj::Problem::findById(id1);
    auto p2 = oj::Problem::findById(id2);
    ASSERT_NE(p1, nullptr);
    ASSERT_NE(p2, nullptr);
    delete p1;
    delete p2;
    p1 = oj::Problem::findById(id1);
    p2 = oj::Problem::findById(id2);
    p1->remove();
    p2->remove();
}

TEST_F(AdminHandlerTest, DBConnectionPoolAvailableAfterMultipleOperations) {
    auto beforePool = oj::ConnectionPool::getInstance().getAvailableCount();

    for (int i = 0; i < 5; ++i) {
        int pid = createTestProblem("Pool Test " + std::to_string(i));
        auto p = oj::Problem::findById(pid);
        ASSERT_NE(p, nullptr);
        delete p;
        p = oj::Problem::findById(pid);
        p->remove();
    }

    auto afterPool = oj::ConnectionPool::getInstance().getAvailableCount();
    EXPECT_EQ(afterPool, beforePool);
}

} // namespace