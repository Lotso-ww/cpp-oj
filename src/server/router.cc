#include "router.h"
#include "../handler/admin_handler.h"
#include "../handler/problem_handler.h"
#include "../handler/auth_handler.h"
#include "../handler/submit_handler.h"
#include "../utils/httplib.h"

namespace oj {

void setupRoutes(httplib::Server& svr) {
    svr.Post("/api/admin/problems", [](const httplib::Request& req, httplib::Response& res) {
        AdminHandler::createProblem(req, res);
    });

    svr.Delete("/api/admin/problems/:id", [](const httplib::Request& req, httplib::Response& res) {
        AdminHandler::deleteProblem(req, res);
    });

    svr.Get("/api/problems", [](const httplib::Request& req, httplib::Response& res) {
        ProblemHandler::listProblems(req, res);
    });

    svr.Get("/api/problems/:id", [](const httplib::Request& req, httplib::Response& res) {
        ProblemHandler::getProblem(req, res);
    });

    svr.Post("/api/submit", [](const httplib::Request& req, httplib::Response& res) {
        SubmitHandler::submitCode(req, res);
    });

    svr.Post("/api/login", [](const httplib::Request& req, httplib::Response& res) {
        AuthHandler::login(req, res);
    });

    svr.Post("/api/logout", [](const httplib::Request& req, httplib::Response& res) {
        AuthHandler::logout(req, res);
    });

    svr.Post("/api/register", [](const httplib::Request& req, httplib::Response& res) {
        AuthHandler::registerUser(req, res);
    });

    svr.Get("/api/me", [](const httplib::Request& req, httplib::Response& res) {
        AuthHandler::me(req, res);
    });

    svr.set_mount_point("/", "./public");

    svr.set_error_handler([](const httplib::Request& req, httplib::Response& res) {
        (void)req;
        if (res.status == -1) {
            res.status = 404;
            res.set_content("{\"error\":\"Not found\"}", "application/json");
        }
    });
}

}

httplib::Server& Router::setupServer(httplib::Server& svr) {
    oj::setupRoutes(svr);
    return svr;
}