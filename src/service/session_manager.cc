#include "session_manager.h"
#include "session.h"
#include <random>
#include <mutex>

namespace oj {

SessionManager& SessionManager::getInstance() {
    static SessionManager instance;
    return instance;
}

SessionManager::SessionManager()
    : tokenChars_("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789"),
      rng_(std::random_device{}()) {
}

std::string SessionManager::generateToken() {
    std::uniform_int_distribution<> dist(0, static_cast<int>(tokenChars_.size() - 1));

    std::string token;
    token.reserve(32);

    for (int i = 0; i < 32; ++i) {
        token += tokenChars_[dist(rng_)];
    }

    return token;
}

std::string SessionManager::createSession(int userId, const std::string& username, const std::string& role) {
    std::lock_guard<std::mutex> lock(mutex_);

    std::string token = generateToken();

    auto session = std::make_unique<Session>(token, userId, username, role);
    sessions_[token] = std::move(session);

    return token;
}

Session* SessionManager::getSession(const std::string& token) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = sessions_.find(token);
    if (it == sessions_.end()) {
        return nullptr;
    }

    Session* session = it->second.get();
    if (session->isExpired()) {
        sessions_.erase(it);
        return nullptr;
    }

    return session;
}

bool SessionManager::destroySession(const std::string& token) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = sessions_.find(token);
    if (it == sessions_.end()) {
        return false;
    }

    sessions_.erase(it);
    return true;
}

void SessionManager::cleanExpiredSessions() {
    std::lock_guard<std::mutex> lock(mutex_);

    for (auto it = sessions_.begin(); it != sessions_.end(); ) {
        if (it->second->isExpired()) {
            it = sessions_.erase(it);
        } else {
            ++it;
        }
    }
}

}
