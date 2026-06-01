#include "config.h"
#include <fstream>
#include <yaml-cpp/yaml.h>

namespace oj {

struct Config::Impl {
    std::string databaseHost;
    int databasePort;
    std::string databaseUsername;
    std::string databasePassword;
    std::string databaseName;

    std::string serverHost;
    int serverPort;

    int connectionPoolSize;
    int requestTimeout;
    int compileTimeout;
    int runTimeout;

    std::string logLevel;
    std::string logFile;
};

Config::Config() : pImpl(std::make_unique<Impl>()) {
    reset();
}

Config& Config::getInstance() {
    static Config instance;
    return instance;
}

void Config::reset() {
    pImpl->databaseHost.clear();
    pImpl->databasePort = 0;
    pImpl->databaseUsername.clear();
    pImpl->databasePassword.clear();
    pImpl->databaseName.clear();

    pImpl->serverHost.clear();
    pImpl->serverPort = 0;

    pImpl->connectionPoolSize = 0;
    pImpl->requestTimeout = 0;
    pImpl->compileTimeout = 0;
    pImpl->runTimeout = 0;

    pImpl->logLevel.clear();
    pImpl->logFile.clear();
}

bool Config::load(const std::string& configPath) {
    try {
        std::ifstream file(configPath);
        if (!file.is_open()) {
            return false;
        }
        
        YAML::Node config = YAML::Load(file);
        
        if (config["database"]) {
            pImpl->databaseHost = config["database"]["host"].as<std::string>();
            pImpl->databasePort = config["database"]["port"].as<int>();
            pImpl->databaseUsername = config["database"]["username"].as<std::string>();
            pImpl->databasePassword = config["database"]["password"].as<std::string>();
            pImpl->databaseName = config["database"]["name"].as<std::string>();
        }
        
        if (config["server"]) {
            pImpl->serverHost = config["server"]["host"].as<std::string>();
            pImpl->serverPort = config["server"]["port"].as<int>();
        }
        
        if (config["connection_pool"]) {
            pImpl->connectionPoolSize = config["connection_pool"]["size"].as<int>();
        }
        
        if (config["timeouts"]) {
            pImpl->requestTimeout = config["timeouts"]["request"].as<int>();
            pImpl->compileTimeout = config["timeouts"]["compile"].as<int>();
            pImpl->runTimeout = config["timeouts"]["run"].as<int>();
        }
        
        if (config["logging"]) {
            pImpl->logLevel = config["logging"]["level"].as<std::string>();
            pImpl->logFile = config["logging"]["file"].as<std::string>();
        }
        
        return true;
    } catch (const YAML::Exception& e) {
        return false;
    }
}

std::string Config::getDatabaseHost() const { return pImpl->databaseHost; }
int Config::getDatabasePort() const { return pImpl->databasePort; }
std::string Config::getDatabaseUsername() const { return pImpl->databaseUsername; }
std::string Config::getDatabasePassword() const { return pImpl->databasePassword; }
std::string Config::getDatabaseName() const { return pImpl->databaseName; }

std::string Config::getServerHost() const { return pImpl->serverHost; }
int Config::getServerPort() const { return pImpl->serverPort; }

int Config::getConnectionPoolSize() const { return pImpl->connectionPoolSize; }
int Config::getRequestTimeout() const { return pImpl->requestTimeout; }
int Config::getCompileTimeout() const { return pImpl->compileTimeout; }
int Config::getRunTimeout() const { return pImpl->runTimeout; }

std::string Config::getLogLevel() const { return pImpl->logLevel; }
std::string Config::getLogFile() const { return pImpl->logFile; }

} // namespace oj