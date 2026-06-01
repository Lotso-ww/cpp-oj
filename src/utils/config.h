#ifndef CONFIG_H
#define CONFIG_H

#include <string>
#include <memory>

namespace oj {

class Config {
public:
    static Config& getInstance();

    bool load(const std::string& configPath);
    void reset();

    std::string getDatabaseHost() const;
    int getDatabasePort() const;
    std::string getDatabaseUsername() const;
    std::string getDatabasePassword() const;
    std::string getDatabaseName() const;

    std::string getServerHost() const;
    int getServerPort() const;

    int getConnectionPoolSize() const;
    int getRequestTimeout() const;
    int getCompileTimeout() const;
    int getRunTimeout() const;

    std::string getLogLevel() const;
    std::string getLogFile() const;

private:
    Config();
    ~Config() = default;
    Config(const Config&) = delete;
    Config& operator=(const Config&) = delete;

    struct Impl;
    std::unique_ptr<Impl> pImpl;
};

} // namespace oj

#endif // CONFIG_H