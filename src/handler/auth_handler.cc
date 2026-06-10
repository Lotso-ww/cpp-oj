#include "auth_handler.h"
#include "../service/auth_service.h"
#include "../service/session_manager.h"
#include "../utils/logger.h"
#include <json/json.h>

namespace oj {

static const char* SESSION_COOKIE_NAME = "oj_session";
// 24h — must match Session's expiresAt (createdAt + 24*3600).
static const int SESSION_MAX_AGE_SECONDS = 24 * 3600;

/* -------------------------------------------------------------------------
 * Cookie parsing
 *
 * Returns the value of `key` from a Cookie header (e.g. "a=1; oj_session=abc;
 * b=2"). Empty string if not found. Trims surrounding whitespace from both
 * name and value (browsers and proxies sometimes insert extra spaces).
 *
 * Note: this is exact-key matching, not substring search. A previous bug
 * matched `oj_session` as a prefix of `oj_session_foo`; the new implementation
 * walks the cookie list and only accepts an exact `key=` followed by either
 * `;`, end-of-string, or whitespace.
 * ----------------------------------------------------------------------- */
static std::string parseCookieValue(const std::string& cookieHeader, const std::string& key) {
    if (cookieHeader.empty() || key.empty()) return "";

    size_t i = 0;
    const size_t n = cookieHeader.size();
    while (i < n) {
        // skip leading whitespace / separators between cookies
        while (i < n && (cookieHeader[i] == ' ' || cookieHeader[i] == ';' || cookieHeader[i] == '\t')) ++i;
        if (i >= n) break;

        // read cookie name up to '=' (cookie names can't contain '=' or whitespace)
        size_t nameStart = i;
        while (i < n && cookieHeader[i] != '=' && cookieHeader[i] != ';' && cookieHeader[i] != ' ' && cookieHeader[i] != '\t') ++i;
        size_t nameEnd = i;

        if (i >= n || cookieHeader[i] != '=') {
            // malformed token, skip to next ';'
            while (i < n && cookieHeader[i] != ';') ++i;
            continue;
        }
        ++i; // skip '='

        // read cookie value up to ';' or end (cookie values may contain spaces
        // per RFC 6265, but in practice for the session token we never have
        // spaces; we tolerate trailing whitespace before the next separator).
        size_t valueStart = i;
        while (i < n && cookieHeader[i] != ';') ++i;
        size_t valueEnd = i;

        if (nameEnd - nameStart == key.size() &&
            cookieHeader.compare(nameStart, key.size(), key) == 0) {
            // Trim trailing whitespace from the value
            while (valueEnd > valueStart &&
                   (cookieHeader[valueEnd - 1] == ' ' || cookieHeader[valueEnd - 1] == '\t')) {
                --valueEnd;
            }
            return cookieHeader.substr(valueStart, valueEnd - valueStart);
        }
    }
    return "";
}

/* Build a Set-Cookie header value for the session cookie.
 * - HttpOnly: prevents JS access (XSS mitigation)
 * - SameSite=Lax: cookie sent on top-level navigations (A4 — was Strict,
 *   which broke legitimate cross-link flows while still being vulnerable to
 *   subdomain takeover). Lax is the modern default.
 * - Max-Age=SESSION_MAX_AGE_SECONDS: matches server-side TTL so client
 *   and server agree on expiry (B2).
 */
static std::string buildSessionCookie(const std::string& token) {
    std::string c = std::string(SESSION_COOKIE_NAME) + "=" + token
        + "; Path=/"
        + "; HttpOnly"
        + "; SameSite=Lax"
        + "; Max-Age=" + std::to_string(SESSION_MAX_AGE_SECONDS);
    return c;
}

static std::string buildClearSessionCookie() {
    std::string c = std::string(SESSION_COOKIE_NAME) + "="
        + "; Path=/"
        + "; HttpOnly"
        + "; SameSite=Lax"
        + "; Max-Age=0";
    return c;
}

void AuthHandler::login(const httplib::Request& req, httplib::Response& res) {
    Json::Value json;
    Json::Reader reader;

    if (!reader.parse(req.body, json) || !json.isObject()) {
        res.status = 400;
        res.set_content(R"({"error":"Invalid JSON"})", "application/json");
        return;
    }

    if (!json.isMember("username") || !json.isMember("password")) {
        res.status = 400;
        res.set_content(R"({"error":"Missing username or password"})", "application/json");
        return;
    }

    std::string username = json["username"].asString();
    std::string password = json["password"].asString();

    int userId = 0;
    std::string role;

    if (!AuthService::login(username, password, userId, role)) {
        res.status = 401;
        res.set_content(R"({"error":"Invalid username or password"})", "application/json");
        return;
    }

    std::string token = AuthService::getSessionToken(userId, username, role);

    res.status = 200;
    res.set_header("Set-Cookie", buildSessionCookie(token));
    res.set_content(R"({"message":"Login successful"})", "application/json");
}

void AuthHandler::logout(const httplib::Request& req, httplib::Response& res) {
    std::string token = parseCookieValue(req.get_header_value("Cookie"), SESSION_COOKIE_NAME);

    if (!token.empty()) {
        AuthService::logout(token);
    }

    res.status = 200;
    res.set_header("Set-Cookie", buildClearSessionCookie());
    res.set_content(R"({"message":"Logout successful"})", "application/json");
}

void AuthHandler::registerUser(const httplib::Request& req, httplib::Response& res) {
    Json::Value json;
    Json::Reader reader;

    if (!reader.parse(req.body, json) || !json.isObject()) {
        res.status = 400;
        res.set_content(R"({"error":"Invalid JSON"})", "application/json");
        return;
    }

    if (!json.isMember("username") || !json.isMember("password")) {
        res.status = 400;
        res.set_content(R"({"error":"Missing username or password"})", "application/json");
        return;
    }

    std::string username = json["username"].asString();
    std::string password = json["password"].asString();

    std::string error;
    if (!AuthService::registerUser(username, password, error)) {
        res.status = 400;
        res.set_content("{\"error\":\"" + error + "\"}", "application/json");
        return;
    }

    res.status = 201;
    res.set_content(R"({"message":"User registered successfully"})", "application/json");
}

void AuthHandler::me(const httplib::Request& req, httplib::Response& res) {
    std::string token = parseCookieValue(req.get_header_value("Cookie"), SESSION_COOKIE_NAME);

    if (token.empty()) {
        res.status = 401;
        res.set_content(R"({"error":"Unauthorized"})", "application/json");
        return;
    }

    int userId = 0;
    std::string username, role;
    if (!AuthService::validateSession(token, userId, username, role)) {
        res.status = 401;
        res.set_content(R"({"error":"Unauthorized"})", "application/json");
        return;
    }

    Json::Value result;
    result["username"] = username;
    result["role"] = role;
    result["userId"] = userId;
    res.status = 200;
    res.set_content(Json::FastWriter().write(result), "application/json");
}

} // namespace oj