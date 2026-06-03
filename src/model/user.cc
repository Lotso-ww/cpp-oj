#include "user.h"
#include "../db/connection_pool.h"
#include "../utils/logger.h"
#include "../utils/password.h"
#include <mysql/mysql.h>
#include <sstream>
#include <iomanip>
#include <cstring>

using namespace LogModule;

namespace oj {

User::User() : id_(0), createdAt_(0) {
}

int User::getId() const {
    return id_;
}

void User::setId(int id) {
    id_ = id;
}

const std::string& User::getUsername() const {
    return username_;
}

void User::setUsername(const std::string& username) {
    username_ = username;
}

const std::string& User::getPassword() const {
    return password_;
}

void User::setPassword(const std::string& password) {
    password_ = password;
}

const std::string& User::getRole() const {
    return role_;
}

void User::setRole(const std::string& role) {
    role_ = role;
}

time_t User::getCreatedAt() const {
    return createdAt_;
}

void User::setCreatedAt(time_t createdAt) {
    createdAt_ = createdAt;
}

User* User::findByUsername(const std::string& username) {
    MYSQL* conn = ConnectionPool::getInstance().getConnection();
    if (!conn) {
        LOG(LogModule::LogLevel::ERROR) << "Failed to get database connection";
        return nullptr;
    }

    std::string query = "SELECT id, username, password, role, created_at FROM users WHERE username = '" + escapeString(conn, username) + "'";
    
    if (mysql_query(conn, query.c_str()) != 0) {
        LOG(LogModule::LogLevel::ERROR) << "Query failed: " << mysql_error(conn);
        ConnectionPool::getInstance().releaseConnection(conn);
        return nullptr;
    }

    MYSQL_RES* result = mysql_store_result(conn);
    if (!result) {
        ConnectionPool::getInstance().releaseConnection(conn);
        return nullptr;
    }

    MYSQL_ROW row = mysql_fetch_row(result);
    if (!row) {
        mysql_free_result(result);
        ConnectionPool::getInstance().releaseConnection(conn);
        return nullptr;
    }

    User* user = new User();
    user->setId(std::stoi(row[0]));
    user->setUsername(row[1] ? row[1] : "");
    user->setPassword(row[2] ? row[2] : "");
    user->setRole(row[3] ? row[3] : "user");
    
    if (row[4]) {
        std::tm tm = {};
        std::istringstream ss(row[4]);
        ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
        if (!ss.fail()) {
            user->setCreatedAt(static_cast<time_t>(std::mktime(&tm)));
        }
    }

    mysql_free_result(result);
    ConnectionPool::getInstance().releaseConnection(conn);
    return user;
}

int User::save() {
    MYSQL* conn = ConnectionPool::getInstance().getConnection();
    if (!conn) {
        LOG(LogModule::LogLevel::ERROR) << "Failed to get database connection";
        return -1;
    }

    std::string hashedPassword = PasswordUtil::hashPassword(password_);
    if (hashedPassword.empty()) {
        LOG(LogModule::LogLevel::ERROR) << "Failed to hash password";
        ConnectionPool::getInstance().releaseConnection(conn);
        return -1;
    }

    std::ostringstream query;
    query << "INSERT INTO users (username, password, role) VALUES ('"
          << escapeString(conn, username_) << "', '"
          << escapeString(conn, hashedPassword) << "', '"
          << escapeString(conn, role_) << "')";

    if (mysql_query(conn, query.str().c_str()) != 0) {
        LOG(LogModule::LogLevel::ERROR) << "Insert failed: " << mysql_error(conn);
        ConnectionPool::getInstance().releaseConnection(conn);
        return -1;
    }

    int id = static_cast<int>(mysql_insert_id(conn));
    id_ = id;
    ConnectionPool::getInstance().releaseConnection(conn);
    return id;
}

bool User::validatePassword(const std::string& password) const {
    return PasswordUtil::verifyPassword(password, password_);
}

bool User::update() {
    MYSQL* conn = ConnectionPool::getInstance().getConnection();
    if (!conn) {
        LOG(LogModule::LogLevel::ERROR) << "Failed to get database connection";
        return false;
    }

    std::ostringstream query;
    query << "UPDATE users SET username = '" << escapeString(conn, username_)
          << "', password = '" << escapeString(conn, password_)
          << "', role = '" << escapeString(conn, role_)
          << "' WHERE id = " << id_;

    if (mysql_query(conn, query.str().c_str()) != 0) {
        LOG(LogModule::LogLevel::ERROR) << "Update failed: " << mysql_error(conn);
        ConnectionPool::getInstance().releaseConnection(conn);
        return false;
    }

    ConnectionPool::getInstance().releaseConnection(conn);
    return true;
}

bool User::remove() {
    MYSQL* conn = ConnectionPool::getInstance().getConnection();
    if (!conn) {
        LOG(LogModule::LogLevel::ERROR) << "Failed to get database connection";
        return false;
    }

    std::string query = "DELETE FROM users WHERE id = " + std::to_string(id_);

    if (mysql_query(conn, query.c_str()) != 0) {
        LOG(LogModule::LogLevel::ERROR) << "Delete failed: " << mysql_error(conn);
        ConnectionPool::getInstance().releaseConnection(conn);
        return false;
    }

    ConnectionPool::getInstance().releaseConnection(conn);
    return true;
}

std::string User::escapeString(MYSQL* conn, const std::string& str) {
    std::string result;
    result.resize(str.size() * 2 + 1);
    mysql_real_escape_string(conn, &result[0], str.c_str(), static_cast<unsigned long>(str.size()));
    result.resize(strlen(result.c_str()));
    return result;
}

}