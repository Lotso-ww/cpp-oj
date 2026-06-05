#pragma once

#include "../utils/httplib.h"

namespace oj {

class AuthHandler {
public:
    static void login(const httplib::Request& req, httplib::Response& res);
    static void logout(const httplib::Request& req, httplib::Response& res);
    static void registerUser(const httplib::Request& req, httplib::Response& res);
    static void me(const httplib::Request& req, httplib::Response& res);
};

}