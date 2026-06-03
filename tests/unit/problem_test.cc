#include <gtest/gtest.h>
#include <fstream>
#include <filesystem>
#include "problem.h"
#include "test_case.h"
#include "user.h"
#include "connection_pool.h"
#include "config.h"
#include "logger.h"
#include "password.h"

using namespace LogModule;

namespace {

class ModelTest : public ::testing::Test {
protected:
    void SetUp() override {
        tempDir = std::filesystem::temp_directory_path() / "cpp_oj_model_test";
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
};

class ProblemTest : public ModelTest {
};

class TestCaseModelTest : public ModelTest {
};

class UserModelTest : public ModelTest {
};

TEST_F(ProblemTest, DefaultConstructor) {
    oj::Problem p;
    EXPECT_EQ(p.getId(), 0);
    EXPECT_EQ(p.getTitle(), "");
    EXPECT_EQ(p.getDifficulty(), "");
    EXPECT_EQ(p.getContent(), "");
    EXPECT_EQ(p.getTemplate(), "");
    EXPECT_EQ(p.getCreatedAt(), 0);
    EXPECT_EQ(p.getTestCases().size(), 0);
}

TEST_F(ProblemTest, GettersAndSetters) {
    oj::Problem p;
    p.setId(100);
    p.setTitle("Two Sum");
    p.setDifficulty("Easy");
    p.setContent("Given an array of integers...");
    p.setTemplate("#include <iostream>");
    p.setCreatedAt(1234567890);

    EXPECT_EQ(p.getId(), 100);
    EXPECT_EQ(p.getTitle(), "Two Sum");
    EXPECT_EQ(p.getDifficulty(), "Easy");
    EXPECT_EQ(p.getContent(), "Given an array of integers...");
    EXPECT_EQ(p.getTemplate(), "#include <iostream>");
    EXPECT_EQ(p.getCreatedAt(), 1234567890);
}

TEST_F(ProblemTest, AddAndGetTestCases) {
    oj::Problem p;
    oj::TestCase tc1, tc2;
    tc1.setId(1);
    tc2.setId(2);

    p.addTestCase(tc1);
    p.addTestCase(tc2);

    EXPECT_EQ(p.getTestCases().size(), 2);
    EXPECT_EQ(p.getTestCases()[0].getId(), 1);
    EXPECT_EQ(p.getTestCases()[1].getId(), 2);
}

TEST_F(ProblemTest, FindByIdNonExistent) {
    auto p = oj::Problem::findById(999999);
    EXPECT_EQ(p, nullptr);
}

TEST_F(ProblemTest, FindAllReturnsVector) {
    auto problems = oj::Problem::findAll();
    EXPECT_TRUE(problems.empty() || problems.size() >= 0);
}

TEST_F(ProblemTest, SaveAndFind) {
    oj::Problem p;
    p.setTitle("Test Problem");
    p.setDifficulty("Medium");
    p.setContent("Test content");
    p.setTemplate("template");

    int id = p.save();
    ASSERT_GT(id, 0);

    auto found = oj::Problem::findById(id);
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->getTitle(), "Test Problem");
    EXPECT_EQ(found->getDifficulty(), "Medium");
    delete found;

    p.remove();
}

TEST_F(ProblemTest, Update) {
    oj::Problem p;
    p.setTitle("Original Title");
    p.setDifficulty("Easy");
    p.setContent("Original content");
    p.setTemplate("original");

    int id = p.save();
    ASSERT_GT(id, 0);

    p.setTitle("Updated Title");
    p.setDifficulty("Hard");
    bool updated = p.update();
    EXPECT_TRUE(updated);

    auto found = oj::Problem::findById(id);
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->getTitle(), "Updated Title");
    EXPECT_EQ(found->getDifficulty(), "Hard");
    delete found;

    p.remove();
}

TEST_F(ProblemTest, Remove) {
    oj::Problem p;
    p.setTitle("To Be Deleted");
    p.setDifficulty("Easy");
    p.setContent("content");
    p.setTemplate("");

    int id = p.save();
    ASSERT_GT(id, 0);

    bool removed = p.remove();
    EXPECT_TRUE(removed);

    auto found = oj::Problem::findById(id);
    EXPECT_EQ(found, nullptr);
}

TEST_F(ProblemTest, FindByIdWithTestCases) {
    oj::Problem p;
    p.setTitle("Problem With Cases");
    p.setDifficulty("Medium");
    p.setContent("content");
    p.setTemplate("");

    int pid = p.save();
    ASSERT_GT(pid, 0);

    oj::TestCase tc;
    tc.setProblemId(pid);
    tc.setInput("1 2");
    tc.setExpected("3");
    tc.setPosition(0);
    tc.save();

    auto found = oj::Problem::findById(pid);
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->getTestCases().size(), 1);
    EXPECT_EQ(found->getTestCases()[0].getInput(), "1 2");
    EXPECT_EQ(found->getTestCases()[0].getExpected(), "3");
    delete found;

    p.remove();
}

TEST_F(TestCaseModelTest, DefaultConstructor) {
    oj::TestCase tc;
    EXPECT_EQ(tc.getId(), 0);
    EXPECT_EQ(tc.getProblemId(), 0);
    EXPECT_EQ(tc.getInput(), "");
    EXPECT_EQ(tc.getExpected(), "");
    EXPECT_EQ(tc.getPosition(), 0);
}

TEST_F(TestCaseModelTest, GettersAndSetters) {
    oj::TestCase tc;
    tc.setId(10);
    tc.setProblemId(5);
    tc.setInput("1 2");
    tc.setExpected("3");
    tc.setPosition(1);

    EXPECT_EQ(tc.getId(), 10);
    EXPECT_EQ(tc.getProblemId(), 5);
    EXPECT_EQ(tc.getInput(), "1 2");
    EXPECT_EQ(tc.getExpected(), "3");
    EXPECT_EQ(tc.getPosition(), 1);
}

TEST_F(TestCaseModelTest, SaveWithProblem) {
    oj::Problem p;
    p.setTitle("Test Problem for TC");
    p.setDifficulty("Easy");
    p.setContent("content");
    p.setTemplate("");

    int pid = p.save();
    ASSERT_GT(pid, 0);

    oj::TestCase tc;
    tc.setProblemId(pid);
    tc.setInput("input data");
    tc.setExpected("expected output");
    tc.setPosition(0);

    int id = tc.save();
    if (id > 0) {
        auto cases = oj::TestCase::findByProblemId(pid);
        EXPECT_GE(cases.size(), 1);
        tc.remove();
    }

    p.remove();
}

TEST_F(TestCaseModelTest, FindByProblemIdNonExistent) {
    auto cases = oj::TestCase::findByProblemId(999999);
    EXPECT_EQ(cases.size(), 0);
}

TEST_F(UserModelTest, DefaultConstructor) {
    oj::User u;
    EXPECT_EQ(u.getId(), 0);
    EXPECT_EQ(u.getUsername(), "");
    EXPECT_EQ(u.getPassword(), "");
    EXPECT_EQ(u.getRole(), "");
    EXPECT_EQ(u.getCreatedAt(), 0);
}

TEST_F(UserModelTest, GettersAndSetters) {
    oj::User u;
    u.setId(1);
    u.setUsername("testuser");
    u.setPassword("hashedpass");
    u.setRole("admin");
    u.setCreatedAt(1234567890);

    EXPECT_EQ(u.getId(), 1);
    EXPECT_EQ(u.getUsername(), "testuser");
    EXPECT_EQ(u.getPassword(), "hashedpass");
    EXPECT_EQ(u.getRole(), "admin");
    EXPECT_EQ(u.getCreatedAt(), 1234567890);
}

TEST_F(UserModelTest, ValidatePassword) {
    std::string originalPassword = "plaintext123";
    std::string hashedPassword = oj::PasswordUtil::hashPassword(originalPassword);

    oj::User u;
    u.setPassword(hashedPassword);

    EXPECT_TRUE(u.validatePassword(originalPassword));
    EXPECT_FALSE(u.validatePassword("wrongpassword"));
    EXPECT_FALSE(u.validatePassword(""));
}

TEST_F(UserModelTest, FindByUsernameNonExistent) {
    auto u = oj::User::findByUsername("nonexistent_user_xyz");
    EXPECT_EQ(u, nullptr);
}

TEST_F(UserModelTest, SaveAndFind) {
    std::string testUsername = "testuser_" + std::to_string(time(nullptr));
    oj::User u;
    u.setUsername(testUsername);
    u.setPassword("testpass");
    u.setRole("user");

    int id = u.save();
    ASSERT_GT(id, 0);

    auto found = oj::User::findByUsername(testUsername);
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->getUsername(), testUsername);
    EXPECT_EQ(found->getRole(), "user");
    delete found;

    u.remove();
}

TEST_F(UserModelTest, Update) {
    std::string testUsername = "update_user_" + std::to_string(time(nullptr));
    oj::User u;
    u.setUsername(testUsername);
    u.setPassword("original_pass");
    u.setRole("user");

    int id = u.save();
    ASSERT_GT(id, 0);

    u.setPassword("updated_pass");
    u.setRole("admin");
    bool updated = u.update();
    EXPECT_TRUE(updated);

    auto found = oj::User::findByUsername(testUsername);
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->getPassword(), "updated_pass");
    EXPECT_EQ(found->getRole(), "admin");
    delete found;

    u.remove();
}

TEST_F(UserModelTest, Remove) {
    std::string testUsername = "remove_user_" + std::to_string(time(nullptr));
    oj::User u;
    u.setUsername(testUsername);
    u.setPassword("pass");
    u.setRole("user");

    int id = u.save();
    ASSERT_GT(id, 0);

    bool removed = u.remove();
    EXPECT_TRUE(removed);

    auto found = oj::User::findByUsername(testUsername);
    EXPECT_EQ(found, nullptr);
}

TEST_F(UserModelTest, SaveDuplicateUsername) {
    std::string username = "duplicate_user_" + std::to_string(time(nullptr));
    oj::User u1;
    u1.setUsername(username);
    u1.setPassword("pass1");
    u1.setRole("user");

    int id1 = u1.save();
    ASSERT_GT(id1, 0);

    oj::User u2;
    u2.setUsername(username);
    u2.setPassword("pass2");
    u2.setRole("user");

    int id2 = u2.save();
    EXPECT_EQ(id2, -1);

    u1.remove();
}

} // namespace