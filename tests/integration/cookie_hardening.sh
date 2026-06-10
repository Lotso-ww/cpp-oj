#!/bin/bash
# ============================================================================
# Cookie hardening integration tests (A4 / B2 / B3).
# Runs against a live server on localhost:8080 with the new cpp-oj binary.
#
# Verifies, end-to-end, that:
#   - Set-Cookie has SameSite=Lax        (A4)
#   - Set-Cookie has Max-Age=86400        (B2)
#   - Cookie parsing uses exact-key match (B3)
# ============================================================================
set -u

BASE="http://localhost:8080"
PASS=0
FAIL=0

assert_contains() {
    # $1=haystack $2=needle $3=label
    if echo "$1" | grep -q -- "$2"; then
        echo "  [PASS] $3"
        PASS=$((PASS+1))
    else
        echo "  [FAIL] $3"
        echo "    expected to contain: $2"
        echo "    actual: $1"
        FAIL=$((FAIL+1))
    fi
}

assert_not_contains() {
    if echo "$1" | grep -q -- "$2"; then
        echo "  [FAIL] $3 (unexpectedly contained: $2)"
        FAIL=$((FAIL+1))
    else
        echo "  [PASS] $3"
        PASS=$((PASS+1))
    fi
}

assert_equals() {
    if [ "$1" = "$2" ]; then
        echo "  [PASS] $3"
        PASS=$((PASS+1))
    else
        echo "  [FAIL] $3 (expected=$2 actual=$1)"
        FAIL=$((FAIL+1))
    fi
}

echo "=== A4/B2: login Set-Cookie attributes ==="
LOGIN_HEADERS=$(curl -s -i -X POST $BASE/api/login \
    -H 'Content-Type: application/json' \
    -d '{"username":"admin","password":"admin123"}')
SET_COOKIE=$(echo "$LOGIN_HEADERS" | grep -i '^Set-Cookie:' | tr -d '\r')
STATUS=$(echo "$LOGIN_HEADERS" | head -1 | awk '{print $2}')

assert_equals "$STATUS" "200" "login returns 200"
assert_contains "$SET_COOKIE" "HttpOnly"       "login cookie has HttpOnly"
assert_contains "$SET_COOKIE" "SameSite=Lax"   "login cookie has SameSite=Lax (A4)"
assert_contains "$SET_COOKIE" "Max-Age=86400"  "login cookie has Max-Age=86400 (B2)"
assert_contains "$SET_COOKIE" "Path=/"         "login cookie has Path=/"
assert_not_contains "$SET_COOKIE" "SameSite=Strict" "login cookie is NOT SameSite=Strict"

# Extract the real token from the Set-Cookie header
REAL_TOKEN=$(echo "$SET_COOKIE" | sed -n 's/.*oj_session=\([^;]*\).*/\1/p')
echo "  [info] real_token=$REAL_TOKEN"

echo ""
echo "=== B3: cookie parsing — exact-key matching ==="

# Case 1: legacy similar-prefix cookie must be ignored, real one honored
RES1=$(curl -s -o /dev/null -w "%{http_code}" $BASE/api/me \
    -H "Cookie: oj_session_legacy=should_be_ignored; oj_session=$REAL_TOKEN")
assert_equals "$RES1" "200" "/api/me accepts exact oj_session, ignores prefix-similar legacy cookie"

# Case 2: only the prefix-similar cookie present → 401
RES2=$(curl -s -o /dev/null -w "%{http_code}" $BASE/api/me \
    -H "Cookie: oj_session_legacy=only_legacy")
assert_equals "$RES2" "401" "/api/me rejects when only prefix-similar cookie present"

# Case 3: cookies in different order, real one first
RES3=$(curl -s -o /dev/null -w "%{http_code}" $BASE/api/me \
    -H "Cookie: oj_session=$REAL_TOKEN; oj_session_legacy=fake")
assert_equals "$RES3" "200" "/api/me handles real-then-legacy order"

# Case 4: many cookies mixed with unrelated ones
RES4=$(curl -s -o /dev/null -w "%{http_code}" $BASE/api/me \
    -H "Cookie: tracker=abc; theme=dark; oj_session=$REAL_TOKEN; lang=zh-CN")
assert_equals "$RES4" "200" "/api/me works when oj_session is in the middle of many cookies"

# Case 5: case-sensitivity check — Cookie header is case-insensitive by HTTP spec,
# httplib preserves case, so a header literally named "cookie" (lowercase) is the
# same key. The session lookup should still work.
RES5=$(curl -s -o /dev/null -w "%{http_code}" $BASE/api/me \
    -H "cookie: oj_session=$REAL_TOKEN")
assert_equals "$RES5" "200" "/api/me accepts lowercase 'cookie' header"

echo ""
echo "=== B2: logout Set-Cookie clears with Max-Age=0 ==="
LOGOUT_HEADERS=$(curl -s -i -X POST $BASE/api/logout \
    -H "Cookie: oj_session=$REAL_TOKEN")
LOGOUT_SET=$(echo "$LOGOUT_HEADERS" | grep -i '^Set-Cookie:' | tr -d '\r')
assert_contains "$LOGOUT_SET" "Max-Age=0"      "logout cookie has Max-Age=0"
assert_contains "$LOGOUT_SET" "SameSite=Lax"   "logout cookie has SameSite=Lax (A4)"
assert_contains "$LOGOUT_SET" "HttpOnly"       "logout cookie has HttpOnly"

echo ""
echo "=== summary ==="
echo "  passed: $PASS"
echo "  failed: $FAIL"
if [ "$FAIL" -gt 0 ]; then
    exit 1
fi
exit 0