#include <iostream>
#include <string>
#include <vector>
#include <mysql/mysql.h>
#include <botan/bcrypt.h>
#include <botan/rng.h>
#include <botan/system_rng.h>

namespace {

const char* DB_HOST = "localhost";
const char* DB_USER = "lotso";
const char* DB_PASS = "";
const char* DB_NAME = "oj_system";
const unsigned int DB_PORT = 3306;

const char* ADMIN_USERNAME = "admin";
const char* ADMIN_PASSWORD = "admin123";

struct TestCaseSeed {
    const char* input;
    const char* expected;
};

struct ProblemSeed {
    const char* title;
    const char* difficulty;
    const char* content;
    const char* codeTemplate;
    std::vector<TestCaseSeed> cases;
};

const std::vector<ProblemSeed> SEED_PROBLEMS = {
    {
        "两数之和",
        "Easy",
        "给定两个整数 a 和 b，输出它们的和。",
        "#include <iostream>\n"
        "using namespace std;\n"
        "int main() {\n"
        "    int a, b;\n"
        "    cin >> a >> b;\n"
        "    cout << a + b << endl;\n"
        "    return 0;\n"
        "}\n",
        {
            {"1 2",     "3"},
            {"100 200", "300"},
        },
    },
    {
        "判断奇偶",
        "Easy",
        "给定一个整数 n，判断它是奇数还是偶数。奇数输出 odd，偶数输出 even。",
        "#include <iostream>\n"
        "using namespace std;\n"
        "int main() {\n"
        "    long long n;\n"
        "    cin >> n;\n"
        "    cout << (n % 2 == 0 ? \"even\" : \"odd\") << endl;\n"
        "    return 0;\n"
        "}\n",
        {
            {"4", "even"},
            {"7", "odd"},
        },
    },
    {
        "判断质数",
        "Medium",
        "给定一个大于 1 的整数 n，判断它是否为质数。是质数输出 yes，否则输出 no。",
        "#include <iostream>\n"
        "using namespace std;\n"
        "int main() {\n"
        "    long long n;\n"
        "    cin >> n;\n"
        "    bool ok = true;\n"
        "    if (n < 2) ok = false;\n"
        "    for (long long i = 2; i * i <= n; ++i) {\n"
        "        if (n % i == 0) { ok = false; break; }\n"
        "    }\n"
        "    cout << (ok ? \"yes\" : \"no\") << endl;\n"
        "    return 0;\n"
        "}\n",
        {
            {"7", "yes"},
            {"9", "no"},
            {"2", "yes"},
        },
    },
    {
        "计算最大公约数",
        "Hard",
        "给定两个正整数 a 和 b，输出它们的最大公约数。",
        "#include <iostream>\n"
        "using namespace std;\n"
        "int main() {\n"
        "    long long a, b;\n"
        "    cin >> a >> b;\n"
        "    while (b != 0) {\n"
        "        long long t = a % b;\n"
        "        a = b;\n"
        "        b = t;\n"
        "    }\n"
        "    cout << a << endl;\n"
        "    return 0;\n"
        "}\n",
        {
            {"12 18",   "6"},
            {"100 75",  "25"},
        },
    },
};

bool execute(MYSQL* conn, const std::string& sql) {
    if (mysql_query(conn, sql.c_str()) != 0) {
        std::cerr << "[FAIL] " << sql << "\n       err: " << mysql_error(conn) << std::endl;
        return false;
    }
    return true;
}

std::string escape(MYSQL* conn, const std::string& str) {
    std::string out;
    out.resize(str.size() * 2 + 1);
    unsigned long len = mysql_real_escape_string(conn, &out[0], str.c_str(),
                                                static_cast<unsigned long>(str.size()));
    out.resize(len);
    return out;
}

std::string hashPassword(const std::string& password) {
    try {
        Botan::System_RNG rng;
        return Botan::generate_bcrypt(password, rng, 10);
    } catch (const std::exception& e) {
        std::cerr << "bcrypt hash failed: " << e.what() << std::endl;
        return "";
    }
}

bool dropAllProblems(MYSQL* conn) {
    if (!execute(conn, "SET FOREIGN_KEY_CHECKS = 0")) return false;
    if (!execute(conn, "TRUNCATE TABLE test_cases")) {
        execute(conn, "SET FOREIGN_KEY_CHECKS = 1");
        return false;
    }
    if (!execute(conn, "TRUNCATE TABLE problems")) {
        execute(conn, "SET FOREIGN_KEY_CHECKS = 1");
        return false;
    }
    if (!execute(conn, "SET FOREIGN_KEY_CHECKS = 1")) return false;
    std::cout << "[OK]   test_cases / problems truncated" << std::endl;
    return true;
}

bool cleanNonAdminUsers(MYSQL* conn) {
    const std::string sql = "DELETE FROM users WHERE username <> 'admin'";
    if (!execute(conn, sql)) return false;

    const my_ulonglong affected = mysql_affected_rows(conn);
    std::cout << "[OK]   removed " << affected << " non-admin user(s)" << std::endl;

    if (!execute(conn, "ALTER TABLE users AUTO_INCREMENT = 1")) return false;
    return true;
}

bool resetAdmin(MYSQL* conn) {
    if (!execute(conn, "DELETE FROM users WHERE username = 'admin'")) return false;
    if (!execute(conn, "ALTER TABLE users AUTO_INCREMENT = 1")) return false;

    const std::string hashed = hashPassword(ADMIN_PASSWORD);
    if (hashed.empty()) {
        std::cerr << "[FAIL] failed to hash admin password" << std::endl;
        return false;
    }

    const std::string escHash = escape(conn, hashed);
    const std::string insert =
        "INSERT INTO users (username, password, role) VALUES ('" +
        std::string(ADMIN_USERNAME) + "', '" + escHash + "', 'admin')";

    if (!execute(conn, insert)) return false;
    std::cout << "[OK]   admin user reset (username=admin, role=admin)" << std::endl;
    return true;
}

bool insertSeedProblems(MYSQL* conn) {
    int easy = 0, medium = 0, hard = 0;
    for (size_t i = 0; i < SEED_PROBLEMS.size(); ++i) {
        const ProblemSeed& p = SEED_PROBLEMS[i];

        const std::string escTitle    = escape(conn, p.title);
        const std::string escContent  = escape(conn, p.content);
        const std::string escTemplate = escape(conn, p.codeTemplate);

        const std::string insertProblem =
            "INSERT INTO problems (title, difficulty, content, template) VALUES ('" +
            escTitle + "', '" + p.difficulty + "', '" + escContent +
            "', '" + escTemplate + "')";

        if (!execute(conn, insertProblem)) return false;

        const my_ulonglong pid = mysql_insert_id(conn);
        std::cout << "[OK]   [" << (i + 1) << "/" << SEED_PROBLEMS.size()
                  << "] id=" << pid << " " << p.difficulty
                  << " \"" << p.title << "\"" << std::endl;

        for (size_t j = 0; j < p.cases.size(); ++j) {
            const std::string escInput    = escape(conn, p.cases[j].input);
            const std::string escExpected = escape(conn, p.cases[j].expected);
            const std::string insertCase =
                "INSERT INTO test_cases (problem_id, input, expected, position) VALUES (" +
                std::to_string(pid) + ", '" + escInput + "', '" + escExpected + "', " +
                std::to_string(static_cast<int>(j)) + ")";
            if (!execute(conn, insertCase)) return false;
        }
        std::cout << "       └─ " << p.cases.size() << " test case(s)" << std::endl;

        if (std::string(p.difficulty) == "Easy")   ++easy;
        if (std::string(p.difficulty) == "Medium") ++medium;
        if (std::string(p.difficulty) == "Hard")   ++hard;
    }

    std::cout << "[OK]   summary: Easy=" << easy
              << " Medium=" << medium
              << " Hard=" << hard << std::endl;
    return true;
}

}

int main() {
    MYSQL* conn = mysql_init(nullptr);
    if (!conn) {
        std::cerr << "mysql_init failed" << std::endl;
        return 1;
    }

    conn = mysql_real_connect(conn, DB_HOST, DB_USER, DB_PASS, DB_NAME,
                              DB_PORT, nullptr, 0);
    if (!conn) {
        std::cerr << "mysql_real_connect failed: " << mysql_error(conn) << std::endl;
        return 1;
    }

    std::cout << "==== OJ test database reset ====" << std::endl;
    std::cout << "target: " << DB_USER << "@" << DB_HOST << ":" << DB_PORT
              << "/" << DB_NAME << std::endl;
    std::cout << "--------------------------------" << std::endl;

    bool ok = true;
    ok = dropAllProblems(conn)      && ok;
    ok = cleanNonAdminUsers(conn)   && ok;
    ok = resetAdmin(conn)           && ok;
    ok = insertSeedProblems(conn)   && ok;

    mysql_close(conn);

    std::cout << "--------------------------------" << std::endl;
    if (ok) {
        std::cout << "[DONE] database is ready for automation tests" << std::endl;
        return 0;
    }
    std::cerr << "[DONE] database reset completed with errors" << std::endl;
    return 1;
}
