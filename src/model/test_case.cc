#include "test_case.h"
#include "../db/connection_pool.h"
#include "../utils/logger.h"
#include <mysql/mysql.h>
#include <cstring>

using namespace LogModule;

namespace oj {

TestCase::TestCase() : id_(0), problemId_(0), position_(0) {
}

int TestCase::getId() const {
    return id_;
}

void TestCase::setId(int id) {
    id_ = id;
}

int TestCase::getProblemId() const {
    return problemId_;
}

void TestCase::setProblemId(int problemId) {
    problemId_ = problemId;
}

const std::string& TestCase::getInput() const {
    return input_;
}

void TestCase::setInput(const std::string& input) {
    input_ = input;
}

const std::string& TestCase::getExpected() const {
    return expected_;
}

void TestCase::setExpected(const std::string& expected) {
    expected_ = expected;
}

int TestCase::getPosition() const {
    return position_;
}

void TestCase::setPosition(int position) {
    position_ = position;
}

std::vector<TestCase> TestCase::findByProblemId(int problemId) {
    std::vector<TestCase> testCases;
    
    MYSQL* conn = ConnectionPool::getInstance().getConnection();
    if (!conn) {
        LOG(LogModule::LogLevel::ERROR) << "Failed to get database connection";
        return testCases;
    }

    std::string query = "SELECT id, problem_id, input, expected, position FROM test_cases WHERE problem_id = " 
                        + std::to_string(problemId) + " ORDER BY position";
    
    if (mysql_query(conn, query.c_str()) != 0) {
        LOG(LogModule::LogLevel::ERROR) << "Query failed: " << mysql_error(conn);
        ConnectionPool::getInstance().releaseConnection(conn);
        return testCases;
    }

    MYSQL_RES* result = mysql_store_result(conn);
    if (!result) {
        ConnectionPool::getInstance().releaseConnection(conn);
        return testCases;
    }

    MYSQL_ROW row;
    while ((row = mysql_fetch_row(result))) {
        TestCase tc;
        tc.setId(std::stoi(row[0]));
        tc.setProblemId(std::stoi(row[1]));
        tc.setInput(row[2] ? row[2] : "");
        tc.setExpected(row[3] ? row[3] : "");
        tc.setPosition(std::stoi(row[4]));
        testCases.push_back(tc);
    }

    mysql_free_result(result);
    ConnectionPool::getInstance().releaseConnection(conn);
    return testCases;
}

int TestCase::save() {
    MYSQL* conn = ConnectionPool::getInstance().getConnection();
    if (!conn) {
        LOG(LogModule::LogLevel::ERROR) << "Failed to get database connection";
        return -1;
    }

    std::ostringstream query;
    query << "INSERT INTO test_cases (problem_id, input, expected, position) VALUES ("
          << problemId_ << ", '"
          << escapeString(conn, input_) << "', '"
          << escapeString(conn, expected_) << "', "
          << position_ << ")";

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

bool TestCase::remove() {
    MYSQL* conn = ConnectionPool::getInstance().getConnection();
    if (!conn) {
        LOG(LogModule::LogLevel::ERROR) << "Failed to get database connection";
        return false;
    }

    std::string query = "DELETE FROM test_cases WHERE id = " + std::to_string(id_);
    
    if (mysql_query(conn, query.c_str()) != 0) {
        LOG(LogModule::LogLevel::ERROR) << "Delete failed: " << mysql_error(conn);
        ConnectionPool::getInstance().releaseConnection(conn);
        return false;
    }

    ConnectionPool::getInstance().releaseConnection(conn);
    return true;
}

std::string TestCase::escapeString(MYSQL* conn, const std::string& str) {
    std::string result;
    result.resize(str.size() * 2 + 1);
    mysql_real_escape_string(conn, &result[0], str.c_str(), static_cast<unsigned long>(str.size()));
    result.resize(strlen(result.c_str()));
    return result;
}

}