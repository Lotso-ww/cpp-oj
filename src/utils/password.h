#pragma once

#include <string>
#include <botan/bcrypt.h>
#include <botan/rng.h>
#include <botan/system_rng.h>

namespace oj {

class PasswordUtil {
public:
    static std::string hashPassword(const std::string& password);
    static bool verifyPassword(const std::string& password, const std::string& hashed);

private:
    static std::string generateSalt();
};

}