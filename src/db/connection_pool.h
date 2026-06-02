#ifndef OJ_CONNECTION_POOL_H
#define OJ_CONNECTION_POOL_H

#include <mysql/mysql.h>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <memory>

namespace oj {

class ConnectionPool {
public:
    static ConnectionPool& getInstance();

    MYSQL* getConnection();
    void releaseConnection(MYSQL* conn);

    void close();

    int getPoolSize() const { return poolSize_; }
    int getAvailableCount() const;

private:
    ConnectionPool();
    ~ConnectionPool();

    ConnectionPool(const ConnectionPool&) = delete;
    ConnectionPool& operator=(const ConnectionPool&) = delete;

    void initPool();
    MYSQL* createConnection();

    int poolSize_;
    std::queue<MYSQL*> availableConnections_;
    std::mutex mutex_;
    std::condition_variable cv_;
    bool isClosing_;
};

MYSQL* getConnection();
void releaseConnection(MYSQL* conn);

}

#endif
