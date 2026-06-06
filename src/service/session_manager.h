#pragma once

#include "session.h"
#include <string>
#include <memory>
#include <ctime>
#include <mutex>
#include <random>
#include <unordered_map>

namespace oj {

class SessionManager {
public:
    static SessionManager& getInstance();

    std::string createSession(int userId, const std::string& username, const std::string& role);
    Session* getSession(const std::string& token);
    bool destroySession(const std::string& token);
    void cleanExpiredSessions();

    SessionManager(const SessionManager&) = delete;
    SessionManager& operator=(const SessionManager&) = delete;

private:
    SessionManager();

    std::string generateToken();
    std::string tokenChars_;
    mutable std::mutex mutex_;
    std::mt19937 rng_;
    std::unordered_map<std::string, std::unique_ptr<Session>> sessions_;
};

}
