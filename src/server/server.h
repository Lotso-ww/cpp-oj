#pragma once

#include <string>
#include <memory>

namespace oj {

class Server {
public:
    Server();
    ~Server();

    bool start();
    void stop();

private:
    struct Impl;
    std::unique_ptr<Impl> pImpl;
};

}