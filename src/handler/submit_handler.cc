#include "submit_handler.h"

namespace oj {

void SubmitHandler::submitCode(const httplib::Request& req, httplib::Response& res) {
    res.status = 501;
    res.set_content(R"({"error":"Not implemented"})", "application/json");
}

}