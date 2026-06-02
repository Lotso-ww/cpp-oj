#pragma once

#include "../utils/httplib.h"

namespace oj {

class SubmitHandler {
public:
    static void submitCode(const httplib::Request& req, httplib::Response& res);
};

}