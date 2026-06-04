#include "password.h"
#include "logger.h"
#include <botan/bcrypt.h>
#include <botan/rng.h>
#include <botan/system_rng.h>
#include <iomanip>
#include <sstream>

namespace {

std::string generateRandomSalt() {
    Botan::System_RNG rng;
    std::vector<uint8_t> salt(16);
    rng.randomize(salt.data(), salt.size());

    std::ostringstream oss;
    for (uint8_t b : salt) {
        oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(b);
    }
    return oss.str();
}

}

namespace oj {

std::string PasswordUtil::generateSalt() {
    return generateRandomSalt();
}

std::string PasswordUtil::hashPassword(const std::string& password) {
    try {
        Botan::System_RNG rng;
        return Botan::generate_bcrypt(password, rng, 10);
    } catch (const std::exception& e) {
        LogModule::logger.getInstance()(LogModule::LogLevel::ERROR, __FILE__, __LINE__)
            << "Botan bcrypt hash failed: " << e.what();
        return "";
    }
}

bool PasswordUtil::verifyPassword(const std::string& password, const std::string& hashed) {
    if (hashed.empty() || password.empty()) {
        return false;
    }

    try {
        return Botan::check_bcrypt(password, hashed);
    } catch (const std::exception& e) {
        LogModule::logger.getInstance()(LogModule::LogLevel::ERROR, __FILE__, __LINE__)
            << "Botan bcrypt verify failed: " << e.what();
        return false;
    }
}

}