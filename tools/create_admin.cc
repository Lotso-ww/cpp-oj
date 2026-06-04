#include <iostream>
#include <mysql/mysql.h>
#include <botan/bcrypt.h>
#include <botan/rng.h>
#include <botan/system_rng.h>

std::string hashPassword(const std::string& password) {
    try {
        Botan::System_RNG rng;
        return Botan::generate_bcrypt(password, rng, 10);
    } catch (const std::exception& e) {
        std::cerr << "Botan bcrypt hash failed: " << e.what() << std::endl;
        return "";
    }
}

bool executeQuery(MYSQL* conn, const std::string& query) {
    if (mysql_query(conn, query.c_str()) != 0) {
        std::cerr << "Query failed: " << mysql_error(conn) << std::endl;
        return false;
    }
    return true;
}

std::string escapeString(MYSQL* conn, const std::string& str) {
    std::string result;
    result.resize(str.size() * 2 + 1);
    mysql_real_escape_string(conn, &result[0], str.c_str(), static_cast<unsigned long>(str.size()));
    result.resize(strlen(result.c_str()));
    return result;
}

int main() {
    MYSQL* conn = mysql_init(nullptr);
    if (!conn) {
        std::cerr << "mysql_init failed" << std::endl;
        return 1;
    }
    
    conn = mysql_real_connect(conn, "localhost", "lotso", "", "oj_system", 3306, nullptr, 0);
    if (!conn) {
        std::cerr << "mysql_real_connect failed: " << mysql_error(conn) << std::endl;
        return 1;
    }
    
    std::cout << "Connected to database successfully." << std::endl;
    
    if (!executeQuery(conn, "SELECT id, username, password, role FROM users WHERE username = 'admin'")) {
        mysql_close(conn);
        return 1;
    }
    
    MYSQL_RES* result = mysql_store_result(conn);
    if (result) {
        MYSQL_ROW row = mysql_fetch_row(result);
        if (row) {
            std::cout << "Found existing admin user (id=" << row[0] << "), deleting..." << std::endl;
            mysql_free_result(result);
            
            if (!executeQuery(conn, "DELETE FROM users WHERE username = 'admin'")) {
                mysql_close(conn);
                return 1;
            }
            std::cout << "Deleted existing admin user." << std::endl;
        } else {
            std::cout << "No existing admin user found, will create new one." << std::endl;
            mysql_free_result(result);
        }
    }
    
    std::string hashedPassword = hashPassword("admin123");
    if (hashedPassword.empty()) {
        std::cerr << "Failed to hash password" << std::endl;
        mysql_close(conn);
        return 1;
    }
    
    std::string escapedPassword = escapeString(conn, hashedPassword);
    std::string query = "INSERT INTO users (username, password, role) VALUES ('admin', '" + escapedPassword + "', 'admin')";
    
    if (!executeQuery(conn, query)) {
        mysql_close(conn);
        return 1;
    }
    
    std::cout << "Admin user created successfully with hashed password." << std::endl;
    std::cout << "Hashed password length: " << hashedPassword.length() << std::endl;
    
    mysql_close(conn);
    return 0;
}