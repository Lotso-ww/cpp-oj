#pragma once

#include <string>
#include <ctime>

namespace oj {

class Session {
public:
    Session();
    Session(const std::string& token, int userId, const std::string& username, const std::string& role);

    const std::string& getToken() const;
    void setToken(const std::string& token);

    int getUserId() const;
    void setUserId(int userId);

    const std::string& getUsername() const;
    void setUsername(const std::string& username);

    const std::string& getRole() const;
    void setRole(const std::string& role);

    time_t getCreatedAt() const;
    void setCreatedAt(time_t createdAt);

    time_t getExpiresAt() const;
    void setExpiresAt(time_t expiresAt);

    bool isExpired() const;

private:
    std::string token_;
    int userId_;
    std::string username_;
    std::string role_;
    time_t createdAt_;
    time_t expiresAt_;
};

}
