#pragma once

#include "../utils/httplib.h"

namespace oj {

class ProblemHandler {
public:
    static void listProblems(const httplib::Request& req, httplib::Response& res);
    static void getProblem(const httplib::Request& req, httplib::Response& res);
};

}