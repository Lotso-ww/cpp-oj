#pragma once

#include <functional>
#include <string>
#include "../utils/httplib.h"

namespace oj {

class AdminHandler {
public:
    using Handler = std::function<void(const httplib::Request&, httplib::Response&)>;

    static void createProblem(const httplib::Request& req, httplib::Response& res);
    static void deleteProblem(const httplib::Request& req, httplib::Response& res);
};

}