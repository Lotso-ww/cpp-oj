#include "problem_handler.h"

namespace oj {

void ProblemHandler::listProblems(const httplib::Request& req, httplib::Response& res) {
    res.status = 501;
    res.set_content(R"({"error":"Not implemented"})", "application/json");
}

void ProblemHandler::getProblem(const httplib::Request& req, httplib::Response& res) {
    res.status = 501;
    res.set_content(R"({"error":"Not implemented"})", "application/json");
}

}