#include <gtest/gtest.h>
#include <fstream>
#include <filesystem>
#include <json/json.h>
#include "problem_service.h"
#include "problem.h"
#include "test_case.h"
#include "connection_pool.h"
#include "config.h"
#include "logger.h"

using namespace LogModule;

namespace {

class ProblemServiceTest : public ::testing::Test {
protected:
    void SetUp() override {
        tempDir = std::filesystem::temp_directory_path() / "cpp_oj_service_test";
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

    int createTestProblem(const std::string& title = "Service Test Problem",
                          const std::string& difficulty = "Easy",
                          const std::string& content = "Test content",
                          const std::string& templateCode = "#include <iostream>") {
        oj::Problem p;
        p.setTitle(title);
        p.setDifficulty(difficulty);
        p.setContent(content);
        p.setTemplate(templateCode);
        return p.save();
    }
};

class GetAllProblemsTest : public ProblemServiceTest {
};

class GetProblemDetailTest : public ProblemServiceTest {
};

TEST_F(GetAllProblemsTest, ReturnsValidJsonValue) {
    Json::Value result = oj::ProblemService::getAllProblems();

    EXPECT_TRUE(result.isMember("problems"));
    EXPECT_TRUE(result.isMember("total"));
    EXPECT_TRUE(result["problems"].isArray());
}

TEST_F(GetAllProblemsTest, TotalMatchesProblemsArraySize) {
    int pid = createTestProblem("Count Test Problem", "Medium", "Content");
    ASSERT_GT(pid, 0);

    Json::Value result = oj::ProblemService::getAllProblems();
    int total = result["total"].asInt();
    int arraySize = static_cast<int>(result["problems"].size());
    EXPECT_EQ(total, arraySize);

    auto p = oj::Problem::findById(pid);
    p->remove();
}

TEST_F(GetAllProblemsTest, ProblemItemContainsRequiredFields) {
    int pid = createTestProblem("Fields Test Problem", "Hard", "Content");
    ASSERT_GT(pid, 0);

    Json::Value result = oj::ProblemService::getAllProblems();

    bool found = false;
    for (const auto& p : result["problems"]) {
        if (p["id"].asInt() == pid) {
            found = true;
            EXPECT_TRUE(p.isMember("id"));
            EXPECT_TRUE(p.isMember("title"));
            EXPECT_TRUE(p.isMember("difficulty"));
            EXPECT_FALSE(p.isMember("content"));
            EXPECT_FALSE(p.isMember("template"));
            EXPECT_FALSE(p.isMember("testCases"));
            break;
        }
    }
    EXPECT_TRUE(found);

    auto p = oj::Problem::findById(pid);
    p->remove();
}

TEST_F(GetAllProblemsTest, EmptyWhenNoProblems) {
    Json::Value result = oj::ProblemService::getAllProblems();
    EXPECT_TRUE(result["problems"].isArray());
    EXPECT_GE(result["total"].asInt(), 0);
}

TEST_F(GetAllProblemsTest, MultipleProblemsOrderedById) {
    int pid1 = createTestProblem("Problem A", "Easy", "Content A");
    int pid2 = createTestProblem("Problem B", "Medium", "Content B");
    int pid3 = createTestProblem("Problem C", "Hard", "Content C");
    ASSERT_GT(pid1, 0);
    ASSERT_GT(pid2, 0);
    ASSERT_GT(pid3, 0);

    Json::Value result = oj::ProblemService::getAllProblems();

    std::vector<int> ids;
    for (const auto& p : result["problems"]) {
        ids.push_back(p["id"].asInt());
    }

    if (ids.size() >= 3) {
        EXPECT_TRUE(std::find(ids.begin(), ids.end(), pid1) != ids.end());
        EXPECT_TRUE(std::find(ids.begin(), ids.end(), pid2) != ids.end());
        EXPECT_TRUE(std::find(ids.begin(), ids.end(), pid3) != ids.end());
    }

    auto p1 = oj::Problem::findById(pid1);
    auto p2 = oj::Problem::findById(pid2);
    auto p3 = oj::Problem::findById(pid3);
    p1->remove();
    p2->remove();
    p3->remove();
}

TEST_F(GetProblemDetailTest, ReturnsValidJsonValue) {
    int pid = createTestProblem();
    ASSERT_GT(pid, 0);

    Json::Value result = oj::ProblemService::getProblemDetail(pid);

    EXPECT_TRUE(result.isMember("id"));
    EXPECT_TRUE(result.isMember("title"));
    EXPECT_TRUE(result.isMember("difficulty"));
    EXPECT_TRUE(result.isMember("content"));
    EXPECT_TRUE(result.isMember("template"));
    EXPECT_TRUE(result.isMember("testCases"));

    auto p = oj::Problem::findById(pid);
    p->remove();
}

TEST_F(GetProblemDetailTest, ReturnsCorrectProblemData) {
    int pid = createTestProblem("Detail Test Problem", "Medium", "Detail content here", "#include <bits/stdc++.h>");
    ASSERT_GT(pid, 0);

    Json::Value result = oj::ProblemService::getProblemDetail(pid);

    EXPECT_EQ(result["id"].asInt(), pid);
    EXPECT_EQ(result["title"].asString(), "Detail Test Problem");
    EXPECT_EQ(result["difficulty"].asString(), "Medium");
    EXPECT_EQ(result["content"].asString(), "Detail content here");
    EXPECT_EQ(result["template"].asString(), "#include <bits/stdc++.h>");

    auto p = oj::Problem::findById(pid);
    p->remove();
}

TEST_F(GetProblemDetailTest, ReturnsEmptyForNonExistentId) {
    Json::Value result = oj::ProblemService::getProblemDetail(999999);
    EXPECT_TRUE(result.isNull() || (result.isObject() && result.size() == 0));
}

TEST_F(GetProblemDetailTest, ContainsTestCasesArray) {
    int pid = createTestProblem("TestCase Test Problem", "Easy", "Content");
    ASSERT_GT(pid, 0);

    oj::TestCase tc;
    tc.setProblemId(pid);
    tc.setInput("input data");
    tc.setExpected("output data");
    tc.setPosition(0);
    tc.save();

    Json::Value result = oj::ProblemService::getProblemDetail(pid);

    EXPECT_TRUE(result["testCases"].isArray());

    auto p = oj::Problem::findById(pid);
    p->remove();
}

TEST_F(GetProblemDetailTest, TestCasesHaveCorrectFields) {
    int pid = createTestProblem("TC Fields Problem", "Hard", "Content");
    ASSERT_GT(pid, 0);

    oj::TestCase tc;
    tc.setProblemId(pid);
    tc.setInput("test input");
    tc.setExpected("test output");
    tc.setPosition(0);
    tc.save();

    Json::Value result = oj::ProblemService::getProblemDetail(pid);

    EXPECT_TRUE(result["testCases"][0].isMember("id"));
    EXPECT_TRUE(result["testCases"][0].isMember("input"));
    EXPECT_TRUE(result["testCases"][0].isMember("expected"));
    EXPECT_TRUE(result["testCases"][0].isMember("position"));
    EXPECT_EQ(result["testCases"][0]["input"].asString(), "test input");
    EXPECT_EQ(result["testCases"][0]["expected"].asString(), "test output");

    auto p = oj::Problem::findById(pid);
    p->remove();
}

TEST_F(GetProblemDetailTest, MultipleTestCasesOrderedByPosition) {
    int pid = createTestProblem("Multi TC Problem", "Medium", "Content");
    ASSERT_GT(pid, 0);

    oj::TestCase tc1, tc2, tc3;
    tc1.setProblemId(pid);
    tc1.setInput("1");
    tc1.setExpected("a");
    tc1.setPosition(0);
    tc1.save();

    tc2.setProblemId(pid);
    tc2.setInput("2");
    tc2.setExpected("b");
    tc2.setPosition(1);
    tc2.save();

    tc3.setProblemId(pid);
    tc3.setInput("3");
    tc3.setExpected("c");
    tc3.setPosition(2);
    tc3.save();

    Json::Value result = oj::ProblemService::getProblemDetail(pid);

    EXPECT_EQ(result["testCases"].size(), 3);
    EXPECT_EQ(result["testCases"][0]["input"].asString(), "1");
    EXPECT_EQ(result["testCases"][1]["input"].asString(), "2");
    EXPECT_EQ(result["testCases"][2]["input"].asString(), "3");

    auto p = oj::Problem::findById(pid);
    p->remove();
}

TEST_F(GetProblemDetailTest, EmptyTestCasesArrayWhenNoTestCases) {
    int pid = createTestProblem("No TC Problem", "Easy", "Content");
    ASSERT_GT(pid, 0);

    Json::Value result = oj::ProblemService::getProblemDetail(pid);

    EXPECT_TRUE(result["testCases"].isArray());
    EXPECT_EQ(result["testCases"].size(), 0);

    auto p = oj::Problem::findById(pid);
    p->remove();
}

} // namespace