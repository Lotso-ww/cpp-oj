#ifndef OJ_USER_H
#define OJ_USER_H

#include <string>
#include <ctime>
#include <mysql/mysql.h>

namespace oj {

class User {
public:
    User();

    int getId() const;
    void setId(int id);

    const std::string& getUsername() const;
    void setUsername(const std::string& username);

    const std::string& getPassword() const;
    void setPassword(const std::string& password);

    const std::string& getRole() const;
    void setRole(const std::string& role);

    time_t getCreatedAt() const;
    void setCreatedAt(time_t createdAt);

    static User* findByUsername(const std::string& username);
    int save();
    bool update();
    bool remove();
    bool validatePassword(const std::string& password) const;

private:
    static std::string escapeString(MYSQL* conn, const std::string& str);

    int id_;
    std::string username_;
    std::string password_;
    std::string role_;
    time_t createdAt_;
};

}

#endif