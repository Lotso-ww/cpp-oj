#include <gtest/gtest.h>
#include <fstream>
#include <filesystem>
#include <json/json.h>
#include "problem_handler.h"
#include "problem_service.h"
#include "problem.h"
#include "test_case.h"
#include "connection_pool.h"
#include "config.h"
#include "logger.h"
#include "../src/utils/httplib.h"

using namespace LogModule;

namespace {

class ProblemHandlerTest : public ::testing::Test {
protected:
    void SetUp() override {
        tempDir = std::filesystem::temp_directory_path() / "cpp_oj_handler_test";
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

    httplib::Request createRequest(const std::string& method, const std::string& path) {
        httplib::Request req;
        req.method = method;
        req.path = path;
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

    int createTestProblemInDB(const std::string& title = "Handler Test Problem",
                              const std::string& difficulty = "Easy",
                              const std::string& content = "Test content") {
        oj::Problem p;
        p.setTitle(title);
        p.setDifficulty(difficulty);
        p.setContent(content);
        p.setTemplate("#include <iostream>");
        return p.save();
    }
};

class ListProblemsTest : public ProblemHandlerTest {
};

class GetProblemTest : public ProblemHandlerTest {
};

TEST_F(ListProblemsTest, ReturnsValidJSON) {
    httplib::Request req = createRequest("GET", "/api/problems");
    httplib::Response res;

    oj::ProblemHandler::listProblems(req, res);

    EXPECT_EQ(res.status, 200);
    Json::Value json = parseResponse(res);
    EXPECT_TRUE(json.isMember("problems"));
    EXPECT_TRUE(json.isMember("total"));
    EXPECT_TRUE(json["problems"].isArray());
}

TEST_F(ListProblemsTest, ReturnsEmptyArrayWhenNoProblems) {
    httplib::Request req = createRequest("GET", "/api/problems");
    httplib::Response res;

    oj::ProblemHandler::listProblems(req, res);

    EXPECT_EQ(res.status, 200);
    Json::Value json = parseResponse(res);
    EXPECT_TRUE(json["problems"].isArray());
    EXPECT_TRUE(json["total"].asInt() >= 0);
}

TEST_F(ListProblemsTest, ReturnsExistingProblems) {
    int pid = createTestProblemInDB("List Test Problem", "Medium", "Content");
    ASSERT_GT(pid, 0);

    httplib::Request req = createRequest("GET", "/api/problems");
    httplib::Response res;

    oj::ProblemHandler::listProblems(req, res);

    EXPECT_EQ(res.status, 200);
    Json::Value json = parseResponse(res);
    EXPECT_TRUE(json["problems"].isArray());

    bool found = false;
    for (const auto& p : json["problems"]) {
        if (p["id"].asInt() == pid) {
            found = true;
            EXPECT_EQ(p["title"].asString(), "List Test Problem");
            EXPECT_EQ(p["difficulty"].asString(), "Medium");
            break;
        }
    }
    EXPECT_TRUE(found);

    auto p = oj::Problem::findById(pid);
    p->remove();
}

TEST_F(ListProblemsTest, ListDoesNotContainContentOrTemplate) {
    int pid = createTestProblemInDB("List Fields Test", "Hard", "Full content here");
    ASSERT_GT(pid, 0);

    httplib::Request req = createRequest("GET", "/api/problems");
    httplib::Response res;

    oj::ProblemHandler::listProblems(req, res);

    EXPECT_EQ(res.status, 200);
    Json::Value json = parseResponse(res);

    for (const auto& p : json["problems"]) {
        if (p["id"].asInt() == pid) {
            EXPECT_FALSE(p.isMember("content"));
            EXPECT_FALSE(p.isMember("template"));
            EXPECT_FALSE(p.isMember("testCases"));
            break;
        }
    }

    auto p = oj::Problem::findById(pid);
    p->remove();
}

TEST_F(GetProblemTest, GetProblemMissingId) {
    httplib::Request req = createRequest("GET", "/api/problems/");
    req.path_params.clear();
    httplib::Response res;

    oj::ProblemHandler::getProblem(req, res);

    EXPECT_EQ(res.status, 400);
    Json::Value json = parseResponse(res);
    EXPECT_EQ(json["error"].asString(), "Invalid problem ID");
}

TEST_F(GetProblemTest, GetProblemInvalidIdFormat) {
    httplib::Request req = createRequest("GET", "/api/problems/abc");
    req.path_params["id"] = "abc";
    httplib::Response res;

    oj::ProblemHandler::getProblem(req, res);

    EXPECT_EQ(res.status, 400);
    Json::Value json = parseResponse(res);
    EXPECT_EQ(json["error"].asString(), "Invalid problem ID");
}

TEST_F(GetProblemTest, GetProblemNegativeId) {
    httplib::Request req = createRequest("GET", "/api/problems/-5");
    req.path_params["id"] = "-5";
    httplib::Response res;

    oj::ProblemHandler::getProblem(req, res);

    EXPECT_EQ(res.status, 400);
    Json::Value json = parseResponse(res);
    EXPECT_EQ(json["error"].asString(), "Invalid problem ID");
}

TEST_F(GetProblemTest, GetProblemZeroId) {
    httplib::Request req = createRequest("GET", "/api/problems/0");
    req.path_params["id"] = "0";
    httplib::Response res;

    oj::ProblemHandler::getProblem(req, res);

    EXPECT_EQ(res.status, 400);
    Json::Value json = parseResponse(res);
    EXPECT_EQ(json["error"].asString(), "Invalid problem ID");
}

TEST_F(GetProblemTest, GetProblemNotFound) {
    httplib::Request req = createRequest("GET", "/api/problems/999999");
    req.path_params["id"] = "999999";
    httplib::Response res;

    oj::ProblemHandler::getProblem(req, res);

    EXPECT_EQ(res.status, 404);
    Json::Value json = parseResponse(res);
    EXPECT_EQ(json["error"].asString(), "Problem not found");
}

TEST_F(GetProblemTest, GetProblemSuccess) {
    int pid = createTestProblemInDB("Get Test Problem", "Easy", "Problem content for get test");
    ASSERT_GT(pid, 0);

    httplib::Request req = createRequest("GET", "/api/problems/" + std::to_string(pid));
    req.path_params["id"] = std::to_string(pid);
    httplib::Response res;

    oj::ProblemHandler::getProblem(req, res);

    EXPECT_EQ(res.status, 200);
    Json::Value json = parseResponse(res);
    EXPECT_EQ(json["id"].asInt(), pid);
    EXPECT_EQ(json["title"].asString(), "Get Test Problem");
    EXPECT_EQ(json["difficulty"].asString(), "Easy");
    EXPECT_EQ(json["content"].asString(), "Problem content for get test");
    EXPECT_EQ(json["template"].asString(), "#include <iostream>");

    auto p = oj::Problem::findById(pid);
    p->remove();
}

TEST_F(GetProblemTest, GetProblemContainsAllFields) {
    int pid = createTestProblemInDB("All Fields Test", "Hard", "Full content");
    ASSERT_GT(pid, 0);

    httplib::Request req = createRequest("GET", "/api/problems/" + std::to_string(pid));
    req.path_params["id"] = std::to_string(pid);
    httplib::Response res;

    oj::ProblemHandler::getProblem(req, res);

    EXPECT_EQ(res.status, 200);
    Json::Value json = parseResponse(res);
    EXPECT_TRUE(json.isMember("id"));
    EXPECT_TRUE(json.isMember("title"));
    EXPECT_TRUE(json.isMember("difficulty"));
    EXPECT_TRUE(json.isMember("content"));
    EXPECT_TRUE(json.isMember("template"));
    EXPECT_TRUE(json.isMember("testCases"));

    auto p = oj::Problem::findById(pid);
    p->remove();
}

TEST_F(GetProblemTest, GetProblemWithTestCases) {
    oj::Problem p;
    p.setTitle("Problem with TestCases");
    p.setDifficulty("Medium");
    p.setContent("Test content");
    int pid = p.save();
    ASSERT_GT(pid, 0);

    oj::TestCase tc1, tc2;
    tc1.setProblemId(pid);
    tc1.setInput("1 2");
    tc1.setExpected("3");
    tc1.setPosition(0);
    tc1.save();

    tc2.setProblemId(pid);
    tc2.setInput("5 6");
    tc2.setExpected("11");
    tc2.setPosition(1);
    tc2.save();

    httplib::Request req = createRequest("GET", "/api/problems/" + std::to_string(pid));
    req.path_params["id"] = std::to_string(pid);
    httplib::Response res;

    oj::ProblemHandler::getProblem(req, res);

    EXPECT_EQ(res.status, 200);
    Json::Value json = parseResponse(res);
    EXPECT_TRUE(json["testCases"].isArray());
    EXPECT_EQ(json["testCases"].size(), 2);
    EXPECT_EQ(json["testCases"][0]["input"].asString(), "1 2");
    EXPECT_EQ(json["testCases"][0]["expected"].asString(), "3");
    EXPECT_EQ(json["testCases"][1]["input"].asString(), "5 6");
    EXPECT_EQ(json["testCases"][1]["expected"].asString(), "11");

    auto problem = oj::Problem::findById(pid);
    problem->remove();
}

TEST_F(GetProblemTest, GetProblemWithEmptyTestCases) {
    oj::Problem p;
    p.setTitle("No TestCases Problem");
    p.setDifficulty("Easy");
    p.setContent("Content");
    int pid = p.save();
    ASSERT_GT(pid, 0);

    httplib::Request req = createRequest("GET", "/api/problems/" + std::to_string(pid));
    req.path_params["id"] = std::to_string(pid);
    httplib::Response res;

    oj::ProblemHandler::getProblem(req, res);

    EXPECT_EQ(res.status, 200);
    Json::Value json = parseResponse(res);
    EXPECT_TRUE(json["testCases"].isArray());
    EXPECT_EQ(json["testCases"].size(), 0);

    auto problem = oj::Problem::findById(pid);
    problem->remove();
}

} // namespace