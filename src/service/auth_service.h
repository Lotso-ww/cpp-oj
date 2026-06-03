#pragma once

#include <string>
#include "../model/user.h"

namespace oj {

class AuthService {
public:
    static bool login(const std::string& username, const std::string& password, int& userId, std::string& role);
    static bool registerUser(const std::string& username, const std::string& password, std::string& error);
    static std::string getSessionToken(int userId, const std::string& username, const std::string& role);
    static bool validateSession(const std::string& token, int& userId, std::string& username, std::string& role);
    static bool logout(const std::string& token);
};

}
