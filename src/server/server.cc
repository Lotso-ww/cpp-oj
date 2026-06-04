#include "server.h"
#include "router.h"
#include "../db/connection_pool.h"
#include "../utils/config.h"
#include "../utils/logger.h"
#include <memory>
#include <csignal>

using namespace LogModule;

namespace oj {

struct Server::Impl {
    httplib::Server svr;
    bool running = false;
};

Server::Server() : pImpl(std::make_unique<Impl>()) {
}

Server::~Server() {
    stop();
}

bool Server::start() {
    Config& config = Config::getInstance();
    
    auto port = config.getServerPort();
    auto host = config.getServerHost();
    
    LOG(LogLevel::INFO) << "Initializing connection pool...";
    (void)ConnectionPool::getInstance();
    LOG(LogLevel::INFO) << "Connection pool initialized";
    
    LOG(LogLevel::INFO) << "Setting up routes...";
    Router::setupServer(pImpl->svr);
    LOG(LogLevel::INFO) << "Routes setup complete";
    
    pImpl->running = true;
    
    LOG(LogLevel::INFO) << "Starting server on " << host << ":" << port;
    
    return pImpl->svr.listen(host, port);
}

void Server::stop() {
    if (pImpl->running) {
        pImpl->svr.stop();
        pImpl->running = false;
        LOG(LogLevel::INFO) << "Server stopped";
    }
}

}