#ifndef OJ_TEST_CASE_H
#define OJ_TEST_CASE_H

#include <string>
#include <vector>
#include <mysql/mysql.h>

namespace oj {

class TestCase {
public:
    TestCase();

    int getId() const;
    void setId(int id);

    int getProblemId() const;
    void setProblemId(int problemId);

    const std::string& getInput() const;
    void setInput(const std::string& input);

    const std::string& getExpected() const;
    void setExpected(const std::string& expected);

    int getPosition() const;
    void setPosition(int position);

    static std::vector<TestCase> findByProblemId(int problemId);
    int save();
    bool remove();

private:
    static std::string escapeString(MYSQL* conn, const std::string& str);

    int id_;
    int problemId_;
    std::string input_;
    std::string expected_;
    int position_;
};

}

#endif