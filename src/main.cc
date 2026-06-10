#include "server/server.h"
#include "utils/logger.h"
#include "utils/config.h"
#include "service/session_manager.h"
#include <iostream>
#include <csignal>
#include <cstdlib>

using namespace LogModule;
using namespace oj;

namespace {

Server* g_server = nullptr;

void signalHandler(int signum) {
    if (signum == SIGINT || signum == SIGTERM) {
        LOG(LogLevel::INFO) << "Received signal " << signum << ", shutting down...";
        if (g_server) {
            g_server->stop();
        }
    }
}

}

int main(int argc, char* argv[]) {
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);
    
    std::string configPath = "./config/config.yaml";
    if (argc > 1) {
        configPath = argv[1];
    }
    
    Config& config = Config::getInstance();
    if (!config.load(configPath)) {
        std::cerr << "Failed to load config from " << configPath << std::endl;
        return 1;
    }
    
    InitLogger();
    ENABLE_CONSOLE_LOG_STRATEGY();
    
    LOG(LogLevel::INFO) << "OJ Server starting...";
    LOG(LogLevel::INFO) << "Database: " << config.getDatabaseHost() << ":" << config.getDatabasePort();
    LOG(LogLevel::INFO) << "Server: " << config.getServerHost() << ":" << config.getServerPort();
    
    Server server;
    g_server = &server;

    // Start background session cleanup. Sweeps expired sessions every 60s
    // so the in-memory map doesn't grow unbounded when users churn. The
    // thread is stopped via SessionManager's destructor on program exit.
    oj::SessionManager::getInstance().startCleanupThread(60);

    if (!server.start()) {
        LOG(LogLevel::FATAL) << "Failed to start server";
        oj::SessionManager::getInstance().stopCleanupThread();
        return 1;
    }

    oj::SessionManager::getInstance().stopCleanupThread();
    LOG(LogLevel::INFO) << "OJ Server stopped";
    return 0;
}