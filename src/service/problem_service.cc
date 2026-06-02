#include "problem_service.h"
#include "../model/problem.h"
#include "../model/test_case.h"
#include "../utils/logger.h"

using namespace LogModule;

namespace oj {

Json::Value ProblemService::getAllProblems() {
    Json::Value result;
    std::vector<Problem> problems = Problem::findAll();
    
    Json::Value problemsArray(Json::arrayValue);
    for (const auto& problem : problems) {
        Json::Value p;
        p["id"] = problem.getId();
        p["title"] = problem.getTitle();
        p["difficulty"] = problem.getDifficulty();
        problemsArray.append(p);
    }
    
    result["problems"] = problemsArray;
    result["total"] = static_cast<int>(problems.size());
    return result;
}

Json::Value ProblemService::getProblemDetail(int id) {
    Json::Value result;
    Problem* problem = Problem::findById(id);
    
    if (!problem) {
        return result;
    }
    
    result["id"] = problem->getId();
    result["title"] = problem->getTitle();
    result["difficulty"] = problem->getDifficulty();
    result["content"] = problem->getContent();
    result["template"] = problem->getTemplate();
    
    Json::Value testCasesArray(Json::arrayValue);
    for (const auto& tc : problem->getTestCases()) {
        Json::Value tcJson;
        tcJson["id"] = tc.getId();
        tcJson["input"] = tc.getInput();
        tcJson["expected"] = tc.getExpected();
        tcJson["position"] = tc.getPosition();
        testCasesArray.append(tcJson);
    }
    result["testCases"] = testCasesArray;
    
    delete problem;
    return result;
}

}