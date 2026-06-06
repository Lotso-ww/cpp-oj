#include <gtest/gtest.h>
#include <fstream>
#include <filesystem>
#include "executor_service.h"
#include "config.h"
#include "logger.h"

using namespace LogModule;

namespace {

class ExecutorTest : public ::testing::Test {
protected:
    void SetUp() override {
        tempDir = std::filesystem::temp_directory_path() / "cpp_oj_exec_test";
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

TEST_F(ExecutorTest, CompileSuccess) {
    std::string sourceCode = R"(
#include <iostream>
int main() {
    std::cout << "Hello World" << std::endl;
    return 0;
}
)";

    auto response = oj::ExecutorService::getInstance().compileAndRun(sourceCode, {});

    EXPECT_TRUE(response.compileSuccess);
    EXPECT_EQ(response.result, oj::RunResult::SUCCESS);
}

TEST_F(ExecutorTest, CompileError) {
    std::string sourceCode = R"(
#include <iostream>
int main() {
    std::cout << "Hello World" << std::endl;
    return 0;
}
int main() {}
)";

    auto response = oj::ExecutorService::getInstance().compileAndRun(sourceCode, {});

    EXPECT_FALSE(response.compileSuccess);
    EXPECT_FALSE(response.compileOutput.empty());
}

TEST_F(ExecutorTest, RunWithTestCase) {
    std::string sourceCode = R"(
#include <iostream>
int main() {
    int a, b;
    std::cin >> a >> b;
    std::cout << a + b << std::endl;
    return 0;
}
)";

    std::vector<std::pair<std::string, std::string>> testCases;
    testCases.emplace_back("1 2", "3\n");
    testCases.emplace_back("10 20", "30\n");

    auto response = oj::ExecutorService::getInstance().compileAndRun(sourceCode, testCases);

    EXPECT_TRUE(response.compileSuccess);
    EXPECT_EQ(response.result, oj::RunResult::SUCCESS);
}

TEST_F(ExecutorTest, WrongAnswer) {
    std::string sourceCode = R"(
#include <iostream>
int main() {
    int a, b;
    std::cin >> a >> b;
    std::cout << a - b << std::endl;
    return 0;
}
)";

    std::vector<std::pair<std::string, std::string>> testCases;
    testCases.emplace_back("1 2", "3\n");

    auto response = oj::ExecutorService::getInstance().compileAndRun(sourceCode, testCases);

    EXPECT_TRUE(response.compileSuccess);
    EXPECT_EQ(response.result, oj::RunResult::SUCCESS);
    EXPECT_EQ(response.errorMessage, "Wrong Answer");
}

TEST_F(ExecutorTest, RuntimeError) {
    std::string sourceCode = R"(
#include <iostream>
int main() {
    int a = 10;
    int b = 0;
    std::cout << a / b << std::endl;
    return 0;
}
)";

    std::vector<std::pair<std::string, std::string>> testCases;
    testCases.emplace_back("", "");

    auto response = oj::ExecutorService::getInstance().compileAndRun(sourceCode, testCases);

    EXPECT_TRUE(response.compileSuccess);
    EXPECT_EQ(response.result, oj::RunResult::RUNTIME_ERROR);
}

TEST_F(ExecutorTest, Timeout) {
    std::string sourceCode = R"(
#include <iostream>
#include <thread>
#include <chrono>
int main() {
    std::this_thread::sleep_for(std::chrono::seconds(10));
    std::cout << "Done" << std::endl;
    return 0;
}
)";

    std::vector<std::pair<std::string, std::string>> testCases;
    testCases.emplace_back("", "");

    auto response = oj::ExecutorService::getInstance().compileAndRun(sourceCode, testCases, 10000, 1000);

    EXPECT_TRUE(response.compileSuccess);
    EXPECT_EQ(response.result, oj::RunResult::TIME_LIMIT_EXCEEDED);
}

TEST_F(ExecutorTest, EmptySourceCode) {
    auto response = oj::ExecutorService::getInstance().compileAndRun("", {});

    EXPECT_FALSE(response.compileSuccess);
    EXPECT_EQ(response.errorMessage, "Empty source code");
}

TEST_F(ExecutorTest, SegmentationFault) {
    std::string sourceCode = R"(
#include <iostream>
int main() {
    int* p = nullptr;
    *p = 42;
    return 0;
}
)";

    std::vector<std::pair<std::string, std::string>> testCases;
    testCases.emplace_back("", "");

    auto response = oj::ExecutorService::getInstance().compileAndRun(sourceCode, testCases);

    EXPECT_TRUE(response.compileSuccess);
    EXPECT_EQ(response.result, oj::RunResult::RUNTIME_ERROR);
}

TEST_F(ExecutorTest, ExitCodeNonZero) {
    std::string sourceCode = R"(
#include <stdlib.h>
int main() {
    return 1;
}
)";

    std::vector<std::pair<std::string, std::string>> testCases;
    testCases.emplace_back("", "");
    auto response = oj::ExecutorService::getInstance().compileAndRun(sourceCode, testCases);

    EXPECT_TRUE(response.compileSuccess);
    EXPECT_EQ(response.result, oj::RunResult::RUNTIME_ERROR);
}

TEST_F(ExecutorTest, InputOutputNormal) {
    std::string sourceCode = R"(
#include <iostream>
#include <string>
int main() {
    std::string name;
    std::cin >> name;
    std::cout << "Hello, " << name << "!" << std::endl;
    return 0;
}
)";

    std::vector<std::pair<std::string, std::string>> testCases;
    testCases.emplace_back("World", "Hello, World!\n");
    testCases.emplace_back("OJ", "Hello, OJ!\n");

    auto response = oj::ExecutorService::getInstance().compileAndRun(sourceCode, testCases);

    EXPECT_TRUE(response.compileSuccess);
    EXPECT_EQ(response.result, oj::RunResult::SUCCESS);
    // 全 AC 时也应回填最后一个用例的实际输出（修复后）
    EXPECT_EQ(response.stdout, "Hello, OJ!\n");
}

TEST_F(ExecutorTest, FloatingPointOutput) {
    std::string sourceCode = R"(
#include <iostream>
int main() {
    double pi = 3.14159;
    std::cout.precision(2);
    std::cout << std::fixed << pi << std::endl;
    return 0;
}
)";

    std::vector<std::pair<std::string, std::string>> testCases;
    testCases.emplace_back("", "3.14\n");
    auto response = oj::ExecutorService::getInstance().compileAndRun(sourceCode, testCases);

    EXPECT_TRUE(response.compileSuccess);
    EXPECT_EQ(response.result, oj::RunResult::SUCCESS);
}

TEST_F(ExecutorTest, RecursiveFunctionStackOverflow) {
    std::string sourceCode = R"(
#include <cstdio>
void overflow() {
    char buf[1024];
    overflow();
}
int main() {
    overflow();
    return 0;
}
)";

    std::vector<std::pair<std::string, std::string>> testCases;
    testCases.emplace_back("", "");
    auto response = oj::ExecutorService::getInstance().compileAndRun(sourceCode, testCases, 10000, 1000);

    EXPECT_TRUE(response.compileSuccess);
    EXPECT_TRUE(response.result == oj::RunResult::TIME_LIMIT_EXCEEDED ||
                 response.result == oj::RunResult::RUNTIME_ERROR);
}

} // namespace