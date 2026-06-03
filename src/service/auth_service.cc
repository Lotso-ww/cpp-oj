#include "auth_service.h"
#include "session_manager.h"
#include "../model/user.h"

namespace oj {

bool AuthService::login(const std::string& username, const std::string& password, int& userId, std::string& role) {
    User* user = User::findByUsername(username);
    if (user == nullptr) {
        return false;
    }

    if (!user->validatePassword(password)) {
        delete user;
        return false;
    }

    userId = user->getId();
    role = user->getRole();
    delete user;
    return true;
}

bool AuthService::registerUser(const std::string& username, const std::string& password, std::string& error) {
    if (username.empty() || password.empty()) {
        error = "Username and password cannot be empty";
        return false;
    }

    if (username.length() < 3 || username.length() > 64) {
        error = "Username must be between 3 and 64 characters";
        return false;
    }

    if (password.length() < 6) {
        error = "Password must be at least 6 characters";
        return false;
    }

    User* existingUser = User::findByUsername(username);
    if (existingUser != nullptr) {
        delete existingUser;
        error = "Username already exists";
        return false;
    }

    User newUser;
    newUser.setUsername(username);
    newUser.setPassword(password);
    newUser.setRole("user");

    if (newUser.save() <= 0) {
        error = "Failed to create user";
        return false;
    }

    return true;
}

std::string AuthService::getSessionToken(int userId, const std::string& username, const std::string& role) {
    return SessionManager::getInstance().createSession(userId, username, role);
}

bool AuthService::validateSession(const std::string& token, int& userId, std::string& username, std::string& role) {
    Session* session = SessionManager::getInstance().getSession(token);
    if (session == nullptr) {
        return false;
    }

    userId = session->getUserId();
    username = session->getUsername();
    role = session->getRole();
    return true;
}

bool AuthService::logout(const std::string& token) {
    return SessionManager::getInstance().destroySession(token);
}

}
