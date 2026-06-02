#ifndef OJ_PROBLEM_H
#define OJ_PROBLEM_H

#include <string>
#include <vector>
#include <ctime>
#include <mysql/mysql.h>

namespace oj {

class TestCase;

class Problem {
public:
    Problem();

    int getId() const;
    void setId(int id);

    const std::string& getTitle() const;
    void setTitle(const std::string& title);

    const std::string& getDifficulty() const;
    void setDifficulty(const std::string& difficulty);

    const std::string& getContent() const;
    void setContent(const std::string& content);

    const std::string& getTemplate() const;
    void setTemplate(const std::string& templateCode);

    time_t getCreatedAt() const;
    void setCreatedAt(time_t createdAt);

    void addTestCase(const TestCase& testCase);
    const std::vector<TestCase>& getTestCases() const;
    std::vector<TestCase>& getTestCases();

    static Problem* findById(int id);
    static std::vector<Problem> findAll();
    int save();
    bool update();
    bool remove();

private:
    static std::string escapeString(MYSQL* conn, const std::string& str);

    int id_;
    std::string title_;
    std::string difficulty_;
    std::string content_;
    std::string template_;
    time_t createdAt_;
    std::vector<TestCase> testCases_;
};

}

#endif