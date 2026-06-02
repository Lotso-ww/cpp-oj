#include "connection_pool.h"
#include "config.h"
#include "logger.h"

using namespace LogModule;

namespace oj {

ConnectionPool::ConnectionPool() : poolSize_(Config::getInstance().getConnectionPoolSize()), isClosing_(false) {
    initPool();
}

ConnectionPool::~ConnectionPool() {
    close();
}

ConnectionPool& ConnectionPool::getInstance() {
    static ConnectionPool instance;
    return instance;
}

void ConnectionPool::initPool() {
    for (int i = 0; i < poolSize_; ++i) {
        MYSQL* conn = createConnection();
        if (conn) {
            availableConnections_.push(conn);
        } else {
            LOG(LogModule::LogLevel::ERROR) << "Failed to create MySQL connection " << i;
        }
    }
    LOG(LogModule::LogLevel::INFO) << "Connection pool initialized with " << availableConnections_.size() << " connections";
}

MYSQL* ConnectionPool::createConnection() {
    MYSQL* conn = mysql_init(nullptr);
    if (!conn) {
        LOG(LogModule::LogLevel::ERROR) << "mysql_init failed";
        return nullptr;
    }

    Config& config = Config::getInstance();
    if (!mysql_real_connect(conn,
                            config.getDatabaseHost().c_str(),
                            config.getDatabaseUsername().c_str(),
                            config.getDatabasePassword().c_str(),
                            config.getDatabaseName().c_str(),
                            config.getDatabasePort(),
                            nullptr,
                            0)) {
        LOG(LogModule::LogLevel::ERROR) << "mysql_real_connect failed: " << mysql_error(conn);
        mysql_close(conn);
        return nullptr;
    }

    if (mysql_set_character_set(conn, "utf8mb4") != 0) {
        LOG(LogModule::LogLevel::ERROR) << "mysql_set_character_set failed: " << mysql_error(conn);
        mysql_close(conn);
        return nullptr;
    }

    return conn;
}

MYSQL* ConnectionPool::getConnection() {
    std::unique_lock<std::mutex> lock(mutex_);

    cv_.wait(lock, [this] {
        return !availableConnections_.empty() || isClosing_;
    });

    if (isClosing_) {
        return nullptr;
    }

    MYSQL* conn = availableConnections_.front();
    availableConnections_.pop();

    if (mysql_ping(conn) != 0) {
        LOG(LogModule::LogLevel::WARNING) << "Connection ping failed, recreating";
        mysql_close(conn);
        conn = createConnection();
        if (!conn) {
            return nullptr;
        }
    }

    return conn;
}

void ConnectionPool::releaseConnection(MYSQL* conn) {
    if (!conn) return;

    std::lock_guard<std::mutex> lock(mutex_);

    if (isClosing_) {
        mysql_close(conn);
        return;
    }

    availableConnections_.push(conn);
    cv_.notify_one();
}

int ConnectionPool::getAvailableCount() const {
    std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(mutex_));
    return static_cast<int>(availableConnections_.size());
}

void ConnectionPool::close() {
    std::lock_guard<std::mutex> lock(mutex_);
    isClosing_ = true;
    cv_.notify_all();

    while (!availableConnections_.empty()) {
        MYSQL* conn = availableConnections_.front();
        availableConnections_.pop();
        mysql_close(conn);
    }
    LOG(LogModule::LogLevel::INFO) << "Connection pool closed";
}

MYSQL* getConnection() {
    return ConnectionPool::getInstance().getConnection();
}

void releaseConnection(MYSQL* conn) {
    ConnectionPool::getInstance().releaseConnection(conn);
}

}
