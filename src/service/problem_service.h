#ifndef OJ_PROBLEM_SERVICE_H
#define OJ_PROBLEM_SERVICE_H

#include <string>
#include <vector>
#include <json/json.h>

namespace oj {

class Problem;

class ProblemService {
public:
    static Json::Value getAllProblems();
    static Json::Value getProblemDetail(int id);
};

}

#endif