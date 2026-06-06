#pragma once

#include "../utils/httplib.h"

namespace oj {

class RunHandler {
public:
    // POST /api/run  { code, problemId } -> per-case results (like LeetCode's "Run Code")
    // Returns per-case AC/WA/TLE/RE/MLE/CE status with input/expected/actual
    // for each test case. Unlike /api/submit, this is informational only
    // and does not save a submission record.
    static void runCode(const httplib::Request& req, httplib::Response& res);
};

}
