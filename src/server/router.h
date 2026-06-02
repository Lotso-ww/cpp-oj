#pragma once

#include <string>
#include <unordered_map>
#include "../utils/httplib.h"

class Router {
public:
    using Handler = std::function<void(const std::string&, int)>;

    void get(const std::string& path, Handler handler);
    void post(const std::string& path, Handler handler);
    void put(const std::string& path, Handler handler);
    void del(const std::string& path, Handler handler);

    bool route(const std::string& method, const std::string& path, int sock);

    static httplib::Server& setupServer(httplib::Server& svr);

private:
    struct Route {
        std::string method;
        std::string path;
        Handler handler;
    };

    std::unordered_map<std::string, Route> routes_;
};