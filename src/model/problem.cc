#include "problem.h"
#include "test_case.h"
#include "../db/connection_pool.h"
#include "../utils/logger.h"
#include <mysql/mysql.h>
#include <sstream>
#include <iomanip>
#include <cstring>

using namespace LogModule;

namespace oj {

Problem::Problem() : id_(0), createdAt_(0) {
}

int Problem::getId() const {
    return id_;
}

void Problem::setId(int id) {
    id_ = id;
}

const std::string& Problem::getTitle() const {
    return title_;
}

void Problem::setTitle(const std::string& title) {
    title_ = title;
}

const std::string& Problem::getDifficulty() const {
    return difficulty_;
}

void Problem::setDifficulty(const std::string& difficulty) {
    difficulty_ = difficulty;
}

const std::string& Problem::getContent() const {
    return content_;
}

void Problem::setContent(const std::string& content) {
    content_ = content;
}

const std::string& Problem::getTemplate() const {
    return template_;
}

void Problem::setTemplate(const std::string& templateCode) {
    template_ = templateCode;
}

time_t Problem::getCreatedAt() const {
    return createdAt_;
}

void Problem::setCreatedAt(time_t createdAt) {
    createdAt_ = createdAt;
}

void Problem::addTestCase(const TestCase& testCase) {
    testCases_.push_back(testCase);
}

const std::vector<TestCase>& Problem::getTestCases() const {
    return testCases_;
}

std::vector<TestCase>& Problem::getTestCases() {
    return testCases_;
}

Problem* Problem::findById(int id) {
    MYSQL* conn = ConnectionPool::getInstance().getConnection();
    if (!conn) {
        LOG(LogModule::LogLevel::ERROR) << "Failed to get database connection";
        return nullptr;
    }

    std::string query = "SELECT id, title, difficulty, content, template, created_at FROM problems WHERE id = " + std::to_string(id);
    
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

    Problem* problem = new Problem();
    problem->setId(std::stoi(row[0]));
    problem->setTitle(row[1] ? row[1] : "");
    problem->setDifficulty(row[2] ? row[2] : "");
    problem->setContent(row[3] ? row[3] : "");
    problem->setTemplate(row[4] ? row[4] : "");
    
    if (row[5]) {
        std::tm tm = {};
        std::istringstream ss(row[5]);
        ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
        if (!ss.fail()) {
            problem->setCreatedAt(static_cast<time_t>(std::mktime(&tm)));
        }
    }

    mysql_free_result(result);

    std::vector<TestCase> testCases = TestCase::findByProblemId(id);
    for (const auto& tc : testCases) {
        problem->addTestCase(tc);
    }

    ConnectionPool::getInstance().releaseConnection(conn);
    return problem;
}

std::vector<Problem> Problem::findAll() {
    std::vector<Problem> problems;
    
    MYSQL* conn = ConnectionPool::getInstance().getConnection();
    if (!conn) {
        LOG(LogModule::LogLevel::ERROR) << "Failed to get database connection";
        return problems;
    }

    std::string query = "SELECT id, title, difficulty, content, template, created_at FROM problems ORDER BY id";
    
    if (mysql_query(conn, query.c_str()) != 0) {
        LOG(LogModule::LogLevel::ERROR) << "Query failed: " << mysql_error(conn);
        ConnectionPool::getInstance().releaseConnection(conn);
        return problems;
    }

    MYSQL_RES* result = mysql_store_result(conn);
    if (!result) {
        ConnectionPool::getInstance().releaseConnection(conn);
        return problems;
    }

    MYSQL_ROW row;
    while ((row = mysql_fetch_row(result))) {
        Problem problem;
        problem.setId(std::stoi(row[0]));
        problem.setTitle(row[1] ? row[1] : "");
        problem.setDifficulty(row[2] ? row[2] : "");
        problem.setContent(row[3] ? row[3] : "");
        problem.setTemplate(row[4] ? row[4] : "");
        
        if (row[5]) {
            std::tm tm = {};
            std::istringstream ss(row[5]);
            ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
            if (!ss.fail()) {
                problem.setCreatedAt(static_cast<time_t>(std::mktime(&tm)));
            }
        }
        
        problems.push_back(problem);
    }

    mysql_free_result(result);
    ConnectionPool::getInstance().releaseConnection(conn);
    return problems;
}

int Problem::save() {
    MYSQL* conn = ConnectionPool::getInstance().getConnection();
    if (!conn) {
        LOG(LogModule::LogLevel::ERROR) << "Failed to get database connection";
        return -1;
    }

    std::ostringstream query;
    query << "INSERT INTO problems (title, difficulty, content, template) VALUES ('"
          << escapeString(conn, title_) << "', '"
          << escapeString(conn, difficulty_) << "', '"
          << escapeString(conn, content_) << "', '"
          << escapeString(conn, template_) << "')";

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

bool Problem::update() {
    MYSQL* conn = ConnectionPool::getInstance().getConnection();
    if (!conn) {
        LOG(LogModule::LogLevel::ERROR) << "Failed to get database connection";
        return false;
    }

    std::ostringstream query;
    query << "UPDATE problems SET title = '" << escapeString(conn, title_)
          << "', difficulty = '" << escapeString(conn, difficulty_)
          << "', content = '" << escapeString(conn, content_)
          << "', template = '" << escapeString(conn, template_)
          << "' WHERE id = " << id_;

    if (mysql_query(conn, query.str().c_str()) != 0) {
        LOG(LogModule::LogLevel::ERROR) << "Update failed: " << mysql_error(conn);
        ConnectionPool::getInstance().releaseConnection(conn);
        return false;
    }

    ConnectionPool::getInstance().releaseConnection(conn);
    return true;
}

bool Problem::remove() {
    MYSQL* conn = ConnectionPool::getInstance().getConnection();
    if (!conn) {
        LOG(LogModule::LogLevel::ERROR) << "Failed to get database connection";
        return false;
    }

    std::string query = "DELETE FROM problems WHERE id = " + std::to_string(id_);
    
    if (mysql_query(conn, query.c_str()) != 0) {
        LOG(LogModule::LogLevel::ERROR) << "Delete failed: " << mysql_error(conn);
        ConnectionPool::getInstance().releaseConnection(conn);
        return false;
    }

    ConnectionPool::getInstance().releaseConnection(conn);
    return true;
}

std::string Problem::escapeString(MYSQL* conn, const std::string& str) {
    std::string result;
    result.resize(str.size() * 2 + 1);
    mysql_real_escape_string(conn, &result[0], str.c_str(), static_cast<unsigned long>(str.size()));
    result.resize(strlen(result.c_str()));
    return result;
}

}