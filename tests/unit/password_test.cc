#include <gtest/gtest.h>
#include <random>
#include <fstream>
#include <filesystem>
#include "password.h"
#include "config.h"
#include "logger.h"

using namespace LogModule;

namespace {

class PasswordUtilTest : public ::testing::Test {
protected:
    void SetUp() override {
        tempDir = std::filesystem::temp_directory_path() / "cpp_oj_password_test";
        std::filesystem::create_directories(tempDir);
        configFile = tempDir / "config.yaml";

        std::string configContent = R"(
database:
  host: "localhost"
  port: 3306
  username: "lotso"
  password: ""
  name: "oj_system"

server:
  host: "0.0.0.0"
  port: 8080

connection_pool:
  size: 5

timeouts:
  request: 5000
  compile: 10000
  run: 5000

logging:
  level: "error"
  file: ""
)";
        std::ofstream(configFile) << configContent;
        oj::Config::getInstance().load(configFile.string());
        LogModule::ENABLE_CONSOLE_LOG_STRATEGY();
    }

    void TearDown() override {
        std::filesystem::remove_all(tempDir);
    }

    std::filesystem::path tempDir;
    std::filesystem::path configFile;
};

TEST_F(PasswordUtilTest, HashPasswordReturnsNonEmpty) {
    std::string hashed = oj::PasswordUtil::hashPassword("testpassword");
    EXPECT_FALSE(hashed.empty());
}

TEST_F(PasswordUtilTest, HashPasswordStartsWithBcryptPrefix) {
    std::string hashed = oj::PasswordUtil::hashPassword("testpassword");
    bool startsWithPrefix = (hashed.substr(0, 7) == "$2y$10$") || (hashed.substr(0, 7) == "$2a$10$");
    EXPECT_TRUE(startsWithPrefix) << "Hash should start with $2y$10$ or $2a$10$, got: " << hashed;
}

TEST_F(PasswordUtilTest, HashPasswordLengthIsCorrect) {
    std::string hashed = oj::PasswordUtil::hashPassword("testpassword");
    EXPECT_EQ(hashed.length(), 60u) << "bcrypt hash should be 60 characters";
}

TEST_F(PasswordUtilTest, HashPasswordGeneratesUniqueSalt) {
    std::string hash1 = oj::PasswordUtil::hashPassword("samepassword");
    std::string hash2 = oj::PasswordUtil::hashPassword("samepassword");
    EXPECT_NE(hash1, hash2) << "Same password should produce different hashes due to random salt";
}

TEST_F(PasswordUtilTest, VerifyPasswordCorrectPassword) {
    std::string password = "mysecretpassword123";
    std::string hashed = oj::PasswordUtil::hashPassword(password);
    
    EXPECT_TRUE(oj::PasswordUtil::verifyPassword(password, hashed));
}

TEST_F(PasswordUtilTest, VerifyPasswordWrongPassword) {
    std::string password = "correctpassword";
    std::string hashed = oj::PasswordUtil::hashPassword(password);
    
    EXPECT_FALSE(oj::PasswordUtil::verifyPassword("wrongpassword", hashed));
}

TEST_F(PasswordUtilTest, VerifyPasswordEmptyPassword) {
    std::string hashed = oj::PasswordUtil::hashPassword("somepassword");
    
    EXPECT_FALSE(oj::PasswordUtil::verifyPassword("", hashed));
}

TEST_F(PasswordUtilTest, VerifyPasswordEmptyHash) {
    EXPECT_FALSE(oj::PasswordUtil::verifyPassword("anypassword", ""));
}

TEST_F(PasswordUtilTest, VerifyPasswordBothEmpty) {
    EXPECT_FALSE(oj::PasswordUtil::verifyPassword("", ""));
}

TEST_F(PasswordUtilTest, HashAndVerifyVariousPasswords) {
    std::vector<std::string> passwords = {
        "password",
        "P@ssw0rd!",
        "中文密码",
        "password with spaces",
        "a",
        "verylongpasswordthatismorethanfiftycharacterslongtoensureithandlescorrectly"
    };
    
    for (const auto& pwd : passwords) {
        std::string hashed = oj::PasswordUtil::hashPassword(pwd);
        EXPECT_FALSE(hashed.empty()) << "Hash should not be empty for: " << pwd;
        EXPECT_TRUE(oj::PasswordUtil::verifyPassword(pwd, hashed)) << "Verify should succeed for: " << pwd;
    }
}

TEST_F(PasswordUtilTest, HashPasswordEmptyString) {
    std::string hashed = oj::PasswordUtil::hashPassword("");
    EXPECT_FALSE(hashed.empty()) << "Hash of empty string should not be empty";
    EXPECT_FALSE(oj::PasswordUtil::verifyPassword("", hashed)) << "Empty password verification returns false";
}

TEST_F(PasswordUtilTest, VerifyPasswordCaseSensitive) {
    std::string password = "CaseSensitive";
    std::string hashed = oj::PasswordUtil::hashPassword(password);
    
    EXPECT_TRUE(oj::PasswordUtil::verifyPassword("CaseSensitive", hashed));
    EXPECT_FALSE(oj::PasswordUtil::verifyPassword("casesensitive", hashed));
    EXPECT_FALSE(oj::PasswordUtil::verifyPassword("CASESENSITIVE", hashed));
}

TEST_F(PasswordUtilTest, VerifyPasswordSpecialCharacters) {
    std::string password = "!@#$%^&*()_+-=[]{}|;':\",./<>?";
    std::string hashed = oj::PasswordUtil::hashPassword(password);
    
    EXPECT_TRUE(oj::PasswordUtil::verifyPassword(password, hashed));
}

TEST_F(PasswordUtilTest, SameHashVerifySamePassword) {
    std::string password = "testpassword";
    std::string hashed1 = oj::PasswordUtil::hashPassword(password);
    std::string hashed2 = oj::PasswordUtil::hashPassword(password);
    
    EXPECT_NE(hashed1, hashed2);
    EXPECT_TRUE(oj::PasswordUtil::verifyPassword(password, hashed1));
    EXPECT_TRUE(oj::PasswordUtil::verifyPassword(password, hashed2));
}

TEST_F(PasswordUtilTest, InvalidHashFormatReturnsFalse) {
    EXPECT_FALSE(oj::PasswordUtil::verifyPassword("password", "invalidhashformat"));
    EXPECT_FALSE(oj::PasswordUtil::verifyPassword("password", "12345"));
    EXPECT_FALSE(oj::PasswordUtil::verifyPassword("password", "$"));
}

} // namespace