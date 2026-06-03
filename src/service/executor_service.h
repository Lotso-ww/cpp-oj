#pragma once

#include <string>
#include <vector>

namespace oj {

enum class RunResult {
    SUCCESS,
    RUNTIME_ERROR,
    TIME_LIMIT_EXCEEDED,
    MEMORY_LIMIT_EXCEEDED,
    SYSTEM_ERROR
};

struct ExecutionResponse {
    bool compileSuccess;
    std::string compileOutput;
    int exitCode;
    std::string stdout;
    std::string stderr;
    long executionTimeMs;
    long memoryKb;
    RunResult result;
    std::string errorMessage;
};

class ExecutorService {
public:
    static ExecutorService& getInstance();

    ExecutionResponse compileAndRun(const std::string& sourceCode, 
                                     const std::vector<std::pair<std::string, std::string>>& testCases,
                                     int compileTimeoutMs = 10000,
                                     int runTimeoutMs = 5000);

private:
    ExecutorService() = default;
    ~ExecutorService() = default;
    ExecutorService(const ExecutorService&) = delete;
    ExecutorService& operator=(const ExecutorService&) = delete;

    bool compileCode(const std::string& sourceCode, std::string& execPath, 
                     std::string& compileOutput, int timeoutMs);
    ExecutionResponse runExecutable(const std::string& executablePath,
                                      const std::string& input,
                                      int timeoutMs);
    std::string createTempSourceFile(const std::string& sourceCode);
    std::string getTmpDir();
};

}