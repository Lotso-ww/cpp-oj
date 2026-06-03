#include "session.h"
#include <algorithm>

namespace oj {

Session::Session()
    : userId_(0), createdAt_(0), expiresAt_(0) {
}

Session::Session(const std::string& token, int userId, const std::string& username, const std::string& role)
    : token_(token), userId_(userId), username_(username), role_(role),
      createdAt_(time(nullptr)), expiresAt_(createdAt_ + 24 * 3600) {
}

const std::string& Session::getToken() const {
    return token_;
}

void Session::setToken(const std::string& token) {
    token_ = token;
}

int Session::getUserId() const {
    return userId_;
}

void Session::setUserId(int userId) {
    userId_ = userId;
}

const std::string& Session::getUsername() const {
    return username_;
}

void Session::setUsername(const std::string& username) {
    username_ = username;
}

const std::string& Session::getRole() const {
    return role_;
}

void Session::setRole(const std::string& role) {
    role_ = role;
}

time_t Session::getCreatedAt() const {
    return createdAt_;
}

void Session::setCreatedAt(time_t createdAt) {
    createdAt_ = createdAt;
}

time_t Session::getExpiresAt() const {
    return expiresAt_;
}

void Session::setExpiresAt(time_t expiresAt) {
    expiresAt_ = expiresAt;
}

bool Session::isExpired() const {
    return time(nullptr) > expiresAt_;
}

}
