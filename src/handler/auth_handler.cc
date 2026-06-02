#include "auth_handler.h"

namespace oj {

void AuthHandler::login(const httplib::Request& req, httplib::Response& res) {
    res.status = 501;
    res.set_content(R"({"error":"Not implemented"})", "application/json");
}

void AuthHandler::logout(const httplib::Request& req, httplib::Response& res) {
    res.status = 501;
    res.set_content(R"({"error":"Not implemented"})", "application/json");
}

void AuthHandler::registerUser(const httplib::Request& req, httplib::Response& res) {
    res.status = 501;
    res.set_content(R"({"error":"Not implemented"})", "application/json");
}

}