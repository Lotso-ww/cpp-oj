#include "executor_service.h"
#include "../utils/logger.h"
#include "../utils/config.h"
#include <unistd.h>
#include <sys/wait.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <cerrno>
#include <cstring>
#include <cstdlib>
#include <sstream>
#include <random>
#include <fstream>

using namespace LogModule;

namespace oj {

ExecutorService& ExecutorService::getInstance() {
    static ExecutorService instance;
    return instance;
}

std::string ExecutorService::getTmpDir() {
    return "/tmp/oj_exec";
}

std::string ExecutorService::createTempSourceFile(const std::string& sourceCode) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(100000, 999999);
    
    std::string tmpDir = getTmpDir();
    mkdir(tmpDir.c_str(), 0755);
    
    std::string filename = tmpDir + "/source_" + std::to_string(dis(gen)) + ".cpp";
    
    std::ofstream outFile(filename);
    if (!outFile.is_open()) {
        return "";
    }
    outFile << sourceCode;
    outFile.close();
    
    return filename;
}

bool ExecutorService::compileCode(const std::string& sourceCode, std::string& execPath,
                                    std::string& compileOutput, int timeoutMs) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(100000, 999999);
    
    std::string tmpDir = getTmpDir();
    mkdir(tmpDir.c_str(), 0755);
    
    execPath = tmpDir + "/exec_" + std::to_string(dis(gen));
    std::string sourcePath = createTempSourceFile(sourceCode);
    if (sourcePath.empty()) {
        compileOutput = "Failed to create temporary source file";
        return false;
    }

    int errPipe[2];
    if (pipe(errPipe) < 0) {
        compileOutput = "Pipe creation failed: " + std::string(strerror(errno));
        unlink(sourcePath.c_str());
        return false;
    }

    pid_t pid = fork();
    if (pid < 0) {
        compileOutput = "Fork failed: " + std::string(strerror(errno));
        close(errPipe[0]);
        close(errPipe[1]);
        unlink(sourcePath.c_str());
        return false;
    }

    if (pid == 0) {
        close(errPipe[0]);
        dup2(errPipe[1], STDERR_FILENO);
        close(errPipe[1]);

        execlp("g++", "g++", "-o", execPath.c_str(), sourcePath.c_str(), 
               "-std=c++17", "-O2", "-Wall", nullptr);
        _exit(1);
    }

    close(errPipe[1]);

    fd_set readSet;
    struct timeval tv;
    tv.tv_sec = timeoutMs / 1000;
    tv.tv_usec = (timeoutMs % 1000) * 1000;

    std::string compileErrors;
    bool timeout = false;

    while (true) {
        FD_ZERO(&readSet);
        FD_SET(errPipe[0], &readSet);

        int selectResult = select(errPipe[0] + 1, &readSet, nullptr, nullptr, &tv);
        
        if (selectResult > 0) {
            char buf[4096];
            ssize_t n = read(errPipe[0], buf, sizeof(buf) - 1);
            if (n > 0) {
                buf[n] = '\0';
                compileErrors += buf;
            } else if (n == 0) {
                break;
            }
        } else if (selectResult == 0) {
            timeout = true;
            kill(pid, SIGKILL);
            break;
        } else {
            break;
        }

        int status;
        int ret = wait4(pid, &status, WNOHANG, nullptr);
        if (ret > 0) break;
    }

    close(errPipe[0]);

    int status;
    struct rusage usage;

    if (timeout) {
        wait4(pid, &status, 0, &usage);
    } else {
        wait4(pid, &status, 0, &usage);
    }
    
    unlink(sourcePath.c_str());

    if (WIFEXITED(status)) {
        int exitCode = WEXITSTATUS(status);
        if (exitCode == 0) {
            return true;
        } else {
            compileOutput = compileErrors;
            return false;
        }
    } else if (WIFSIGNALED(status)) {
        compileOutput = "Compilation killed by signal " + std::to_string(WTERMSIG(status));
        unlink(execPath.c_str());
        return false;
    }

    compileOutput = compileErrors;
    return false;
}

ExecutionResponse ExecutorService::runExecutable(const std::string& executablePath,
                                                   const std::string& input,
                                                   int timeoutMs) {
    ExecutionResponse response;
    response.result = RunResult::SYSTEM_ERROR;

    int inputPipe[2], outputPipe[2], errorPipe[2];
    if (pipe(inputPipe) < 0 || pipe(outputPipe) < 0 || pipe(errorPipe) < 0) {
        response.errorMessage = "Pipe creation failed: " + std::string(strerror(errno));
        return response;
    }

    pid_t pid = fork();
    if (pid < 0) {
        response.errorMessage = "Fork failed: " + std::string(strerror(errno));
        close(inputPipe[0]); close(inputPipe[1]);
        close(outputPipe[0]); close(outputPipe[1]);
        close(errorPipe[0]); close(errorPipe[1]);
        return response;
    }

    if (pid == 0) {
        close(inputPipe[1]);
        close(outputPipe[0]);
        close(errorPipe[0]);

        dup2(inputPipe[0], STDIN_FILENO);
        dup2(outputPipe[1], STDOUT_FILENO);
        dup2(errorPipe[1], STDERR_FILENO);

        close(inputPipe[0]);
        close(outputPipe[1]);
        close(errorPipe[1]);

        struct rlimit rl;
        rl.rlim_cur = 64 * 1024 * 1024;
        rl.rlim_max = 128 * 1024 * 1024;
        setrlimit(RLIMIT_AS, &rl);
        rl.rlim_cur = 60;
        rl.rlim_max = 60;
        setrlimit(RLIMIT_CPU, &rl);

        execl(executablePath.c_str(), executablePath.c_str(), nullptr);
        _exit(1);
    }

    close(inputPipe[0]);
    close(outputPipe[1]);
    close(errorPipe[1]);

    if (!input.empty()) {
        write(inputPipe[1], input.c_str(), input.size());
    }
    close(inputPipe[1]);

    fd_set readSet;
    struct timeval tv;
    tv.tv_sec = timeoutMs / 1000;
    tv.tv_usec = (timeoutMs % 1000) * 1000;

    std::string stdoutContent;
    std::string stderrContent;
    bool timeout = false;

    int status;
    struct rusage usage;

    while (true) {
        FD_ZERO(&readSet);
        int maxFd = 0;

        if (outputPipe[0] >= 0) {
            FD_SET(outputPipe[0], &readSet);
            maxFd = std::max(maxFd, outputPipe[0]);
        }
        if (errorPipe[0] >= 0) {
            FD_SET(errorPipe[0], &readSet);
            maxFd = std::max(maxFd, errorPipe[0]);
        }

        int selectResult = select(maxFd + 1, &readSet, nullptr, nullptr, &tv);
        
        if (selectResult > 0) {
            char buf[4096];
            if (FD_ISSET(outputPipe[0], &readSet)) {
                ssize_t n = read(outputPipe[0], buf, sizeof(buf) - 1);
                if (n > 0) {
                    buf[n] = '\0';
                    stdoutContent += buf;
                }
            }
            if (FD_ISSET(errorPipe[0], &readSet)) {
                ssize_t n = read(errorPipe[0], buf, sizeof(buf) - 1);
                if (n > 0) {
                    buf[n] = '\0';
                    stderrContent += buf;
                }
            }
        } else if (selectResult == 0) {
            timeout = true;
            kill(pid, SIGKILL);
            break;
        } else {
            break;
        }

        int ret = wait4(pid, &status, WNOHANG, &usage);
        if (ret > 0) break;
    }

    if (timeout) {
        wait4(pid, &status, 0, &usage);
    } else {
        int ret = wait4(pid, &status, 0, &usage);
        if (ret < 0 && !timeout) {
            response.errorMessage = "Wait failed: " + std::string(strerror(errno));
        }
    }

    close(outputPipe[0]);
    close(errorPipe[0]);

    response.stdout = stdoutContent;
    response.stderr = stderrContent;
    response.executionTimeMs = usage.ru_utime.tv_sec * 1000 + usage.ru_utime.tv_usec / 1000;

    if (WIFEXITED(status)) {
        response.exitCode = WEXITSTATUS(status);
        if (response.exitCode != 0) {
            response.result = RunResult::RUNTIME_ERROR;
        } else {
            response.result = RunResult::SUCCESS;
        }
    } else if (WIFSIGNALED(status)) {
        int sig = WTERMSIG(status);
        if (sig == SIGKILL || sig == SIGXCPU) {
            response.result = RunResult::TIME_LIMIT_EXCEEDED;
            response.errorMessage = "Execution killed due to timeout or resource limit";
        } else {
            response.result = RunResult::RUNTIME_ERROR;
            response.errorMessage = "Execution killed by signal " + std::to_string(sig);
        }
    }

    return response;
}

ExecutionResponse ExecutorService::compileAndRun(const std::string& sourceCode,
                                                  const std::vector<std::pair<std::string, std::string>>& testCases,
                                                  int compileTimeoutMs,
                                                  int runTimeoutMs) {
    ExecutionResponse finalResponse;
    finalResponse.result = RunResult::SYSTEM_ERROR;
    finalResponse.compileSuccess = false;

    if (sourceCode.empty()) {
        finalResponse.errorMessage = "Empty source code";
        return finalResponse;
    }

    int compileTimeout = Config::getInstance().getCompileTimeout();
    if (compileTimeoutMs > 0) {
        compileTimeout = compileTimeoutMs;
    }

    std::string execPath;
    std::string compileOutput;
    bool compileSuccess = compileCode(sourceCode, execPath, compileOutput, compileTimeout);
    
    if (!compileSuccess) {
        finalResponse.compileSuccess = false;
        finalResponse.compileOutput = compileOutput;
        finalResponse.result = RunResult::SYSTEM_ERROR;
        finalResponse.errorMessage = "Compilation failed: " + compileOutput;
        return finalResponse;
    }

    finalResponse.compileSuccess = true;

    if (testCases.empty()) {
        finalResponse.result = RunResult::SUCCESS;
        return finalResponse;
    }

    for (const auto& testCase : testCases) {
        int runTimeout = Config::getInstance().getRunTimeout();
        if (runTimeoutMs > 0) {
            runTimeout = runTimeoutMs;
        }

        ExecutionResponse runResp = runExecutable(execPath, testCase.first, runTimeout);

        if (runResp.result == RunResult::TIME_LIMIT_EXCEEDED) {
            finalResponse.result = RunResult::TIME_LIMIT_EXCEEDED;
            finalResponse.errorMessage = runResp.errorMessage;
            finalResponse.executionTimeMs = runResp.executionTimeMs;
            unlink(execPath.c_str());
            return finalResponse;
        }

        if (runResp.result == RunResult::RUNTIME_ERROR) {
            finalResponse.result = RunResult::RUNTIME_ERROR;
            finalResponse.errorMessage = runResp.errorMessage;
            finalResponse.executionTimeMs = runResp.executionTimeMs;
            finalResponse.stdout = runResp.stdout;
            finalResponse.stderr = runResp.stderr;
            unlink(execPath.c_str());
            return finalResponse;
        }

        std::string expectedOutput = testCase.second;
        if (runResp.stdout != expectedOutput) {
            finalResponse.result = RunResult::SUCCESS;
            finalResponse.errorMessage = "Wrong Answer";
            finalResponse.stdout = runResp.stdout;
            finalResponse.stderr = runResp.stderr;
            finalResponse.executionTimeMs = runResp.executionTimeMs;
            unlink(execPath.c_str());
            return finalResponse;
        }

        // Track the last successful case's output so callers (run_handler
        // in particular) can surface `actual` on AC. The previous behaviour
        // dropped stdout on the all-pass path.
        finalResponse.stdout = runResp.stdout;
        finalResponse.stderr = runResp.stderr;
        finalResponse.executionTimeMs = runResp.executionTimeMs;
    }

    finalResponse.result = RunResult::SUCCESS;
    finalResponse.errorMessage = "";
    unlink(execPath.c_str());

    return finalResponse;
}

}