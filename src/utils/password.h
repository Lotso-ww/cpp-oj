#pragma once

#include <string>

namespace oj {

class PasswordUtil {
public:
    static std::string hashPassword(const std::string& password);
    static bool verifyPassword(const std::string& password, const std::string& hashed);

private:
    static std::string generateSalt();
};

}