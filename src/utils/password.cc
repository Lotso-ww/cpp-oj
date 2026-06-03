#include "password.h"
#include "logger.h"
#include <openssl/rand.h>
#include <openssl/err.h>
#include <unistd.h>
#include <fcntl.h>
#include <random>
#include <sstream>
#include <iomanip>

namespace {

std::string generateRandomSalt() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 255);
    
    unsigned char salt[16];
    for (int i = 0; i < 16; ++i) {
        salt[i] = static_cast<unsigned char>(dis(gen));
    }
    
    std::ostringstream oss;
    for (int i = 0; i < 16; ++i) {
        oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(salt[i]);
    }
    return oss.str();
}

}

namespace oj {

std::string PasswordUtil::generateSalt() {
    return generateRandomSalt();
}

std::string PasswordUtil::hashPassword(const std::string& password) {
    std::string salt = generateSalt();
    std::string saltPrefix = "$2y$10$" + salt;
    
    char* result = crypt(password.c_str(), saltPrefix.c_str());
    
    if (result == nullptr) {
        LogModule::logger.getInstance()(LogModule::LogLevel::ERROR, __FILE__, __LINE__) << "crypt() failed for password hashing";
        return "";
    }
    
    return std::string(result);
}

bool PasswordUtil::verifyPassword(const std::string& password, const std::string& hashed) {
    if (hashed.empty() || password.empty()) {
        return false;
    }
    
    char* result = crypt(password.c_str(), hashed.c_str());
    if (result == nullptr) {
        LogModule::logger.getInstance()(LogModule::LogLevel::ERROR, __FILE__, __LINE__) << "crypt() failed for password verification";
        return false;
    }
    
    return std::string(result) == hashed;
}

}