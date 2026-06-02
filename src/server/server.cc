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
    
    (void)ConnectionPool::getInstance();
    
    InitLogger();
    ENABLE_CONSOLE_LOG_STRATEGY();
    
    LOG(LogLevel::INFO) << "Starting server on " << host << ":" << port;
    
    Router::setupServer(pImpl->svr);
    
    pImpl->running = true;
    
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