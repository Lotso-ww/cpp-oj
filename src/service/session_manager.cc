#include "session_manager.h"
#include "session.h"
#include <random>
#include <mutex>
#include <chrono>

namespace oj {

SessionManager& SessionManager::getInstance() {
    static SessionManager instance;
    return instance;
}

SessionManager::SessionManager()
    : tokenChars_("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789"),
      rng_(std::random_device{}()) {
}

SessionManager::~SessionManager() {
    stopCleanupThread();
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

void SessionManager::startCleanupThread(int intervalSeconds) {
    bool expected = false;
    if (!cleanupRunning_.compare_exchange_strong(expected, true)) {
        return; // already running
    }
    cleanupIntervalSeconds_ = intervalSeconds > 0 ? intervalSeconds : 60;

    cleanupThread_ = std::thread([this]() {
        std::unique_lock<std::mutex> lk(cleanupMutex_);
        while (cleanupRunning_.load()) {
            // Wake either when the interval elapses or when stop is requested.
            cleanupCv_.wait_for(lk, std::chrono::seconds(cleanupIntervalSeconds_),
                                [this] { return !cleanupRunning_.load(); });
            lk.unlock();
            if (!cleanupRunning_.load()) break;
            cleanExpiredSessions();
            lk.lock();
        }
    });
}

void SessionManager::stopCleanupThread() {
    if (!cleanupRunning_.exchange(false)) {
        return; // not running
    }
    {
        std::lock_guard<std::mutex> lk(cleanupMutex_);
        cleanupCv_.notify_all();
    }
    if (cleanupThread_.joinable()) {
        cleanupThread_.join();
    }
}

} // namespace oj