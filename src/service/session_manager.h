#pragma once

#include "session.h"
#include <string>
#include <memory>
#include <ctime>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <random>
#include <thread>
#include <unordered_map>

namespace oj {

class SessionManager {
public:
    static SessionManager& getInstance();

    std::string createSession(int userId, const std::string& username, const std::string& role);
    Session* getSession(const std::string& token);
    bool destroySession(const std::string& token);
    void cleanExpiredSessions();

    // Lifecycle for the background cleanup thread.
    // startCleanupThread() spawns a thread that calls cleanExpiredSessions()
    // every `intervalSeconds` (default 60s). Idempotent — a second call is a no-op.
    // stopCleanupThread() signals the thread to exit and joins it. Safe to call
    // multiple times.
    void startCleanupThread(int intervalSeconds = 60);
    void stopCleanupThread();

    SessionManager(const SessionManager&) = delete;
    SessionManager& operator=(const SessionManager&) = delete;

private:
    SessionManager();
    ~SessionManager();

    std::string generateToken();
    std::string tokenChars_;
    mutable std::mutex mutex_;
    std::mt19937 rng_;
    std::unordered_map<std::string, std::unique_ptr<Session>> sessions_;

    // Background cleanup state.
    std::thread cleanupThread_;
    std::atomic<bool> cleanupRunning_{false};
    int cleanupIntervalSeconds_ = 60;
    std::condition_variable cleanupCv_;
    std::mutex cleanupMutex_;
};

} // namespace oj