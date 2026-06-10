import pytest
import requests
from conftest import BASE_URL


class TestAuthentication:
    @pytest.mark.auth
    def test_1_1_register(self, api_regular, unique_suffix):
        username = f"testuser_{unique_suffix}"
        response = api_regular.register(username, "test123")
        assert response.status_code == 201
        assert response.json().get("message") == "User registered successfully"

    @pytest.mark.auth
    def test_1_2_login_regular_user(self, api_regular, unique_suffix):
        username = f"testuser_{unique_suffix}"
        api_regular.register(username, "test123")
        response, _ = api_regular.login(username, "test123")
        assert response.status_code == 200
        assert response.json().get("message") == "Login successful"

    @pytest.mark.auth
    def test_1_3_logout(self, api_regular, regular_user_cookies):
        response = api_regular.logout(regular_user_cookies)
        assert response.status_code == 200
        assert response.json().get("message") == "Logout successful"

    @pytest.mark.auth
    def test_1_4_login_admin(self, api_admin):
        response, _ = api_admin.login("admin", "admin123")
        assert response.status_code == 200
        assert response.json().get("message") == "Login successful"


class TestProblem:
    @pytest.mark.problem
    def test_2_1_get_problems(self, api):
        response = api.get_problems()
        assert response.status_code == 200
        data = response.json()
        assert "problems" in data
        assert "total" in data

    @pytest.mark.problem
    def test_2_2_get_problem_detail(self, api):
        response = api.get_problem(64)
        assert response.status_code == 200
        data = response.json()
        assert "id" in data
        assert "title" in data
        assert "difficulty" in data
        assert "content" in data
        assert "template" in data
        assert "testCases" in data


class TestSubmission:
    @pytest.fixture
    def problem_with_test_cases(self, api_admin, admin_cookies, timestamp, unique_suffix):
        problem_data = {
            "title": f"提交测试题_{timestamp}_{unique_suffix}",
            "difficulty": "Easy",
            "content": "计算a+b",
            "template": "#include <iostream>",
            "testCases": [
                {"input": "1 2", "expected": "3"},
                {"input": "5 7", "expected": "12"},
                {"input": "0 0", "expected": "0"}
            ]
        }
        response = api_admin.create_problem(problem_data, admin_cookies)
        return response.json().get("id")

    @pytest.mark.submission
    @pytest.mark.slow
    def test_3_1_submit_ac(self, api_admin, admin_cookies, problem_with_test_cases):
        code = "#include <iostream>\nusing namespace std;\nint main() { int a,b; cin>>a>>b; cout<<a+b; return 0; }"
        response = api_admin.submit_code(code, problem_with_test_cases, admin_cookies)
        assert response.status_code == 200
        data = response.json()
        assert data.get("status") == "AC"
        assert "stdout" in data
        assert "executionTimeMs" in data

    @pytest.mark.submission
    @pytest.mark.slow
    def test_3_2_submit_ce(self, api_admin, admin_cookies, problem_with_test_cases):
        code = "#include <iostream>\nint main() { int x = undefined_variable; return 0; }"
        response = api_admin.submit_code(code, problem_with_test_cases, admin_cookies)
        assert response.status_code == 200
        data = response.json()
        assert data.get("status") == "CE"
        assert "compileOutput" in data
        assert "error" in data
        assert "Compilation failed" in data.get("error")
        assert "undefined_variable" in data.get("compileOutput")

    @pytest.mark.submission
    @pytest.mark.slow
    def test_3_3_submit_tle(self, api_admin, admin_cookies, problem_with_test_cases):
        code = "#include <iostream>\nusing namespace std;\nint main() { while(1) { } return 0; }"
        response = api_admin.submit_code(code, problem_with_test_cases, admin_cookies)
        assert response.status_code == 200
        data = response.json()
        assert data.get("status") == "TLE"


class TestAdmin:
    @pytest.fixture
    def problem_without_test_cases(self, api_admin, admin_cookies, timestamp, unique_suffix):
        problem_data = {
            "title": f"无测试用例题目_{timestamp}_{unique_suffix}",
            "difficulty": "Easy",
            "content": "test",
            "template": "#include <iostream>",
            "testCases": []
        }
        response = api_admin.create_problem(problem_data, admin_cookies)
        return response.json().get("id")

    @pytest.mark.admin
    def test_4_1_create_problem(self, api_admin, admin_cookies, timestamp, unique_suffix):
        problem_data = {
            "title": f"自动化测试题_{timestamp}_{unique_suffix}",
            "difficulty": "Easy",
            "content": "计算a+b",
            "template": "#include <iostream>",
            "testCases": [{"input": "1 2", "expected": "3"}]
        }
        response = api_admin.create_problem(problem_data, admin_cookies)
        assert response.status_code == 201
        data = response.json()
        assert "id" in data

    @pytest.mark.admin
    def test_4_2_delete_problem(self, api_admin, admin_cookies, timestamp, unique_suffix):
        problem_data = {
            "title": f"待删除题目_{timestamp}_{unique_suffix}",
            "difficulty": "Easy",
            "content": "test",
            "template": "#include <iostream>",
            "testCases": [{"input": "1", "expected": "1"}]
        }
        create_response = api_admin.create_problem(problem_data, admin_cookies)
        assert create_response.status_code == 201
        problem_id = create_response.json().get("id")

        delete_response = api_admin.delete_problem(problem_id, admin_cookies)
        assert delete_response.status_code == 200
        assert delete_response.json().get("message") == "Problem deleted successfully"

    @pytest.mark.admin
    def test_4_3_delete_nonexistent_problem(self, api_admin, admin_cookies):
        response = api_admin.delete_problem(99999, admin_cookies)
        assert response.status_code == 404
        assert response.json().get("error") == "Problem not found"

    @pytest.mark.admin
    def test_4_4_create_problem_with_multiple_test_cases(self, api_admin, admin_cookies, timestamp, unique_suffix):
        problem_data = {
            "title": f"多测试点题目_{timestamp}_{unique_suffix}",
            "difficulty": "Medium",
            "content": "计算a+b",
            "template": "#include <iostream>",
            "testCases": [
                {"input": "1 2", "expected": "3"},
                {"input": "10 20", "expected": "30"},
                {"input": "-5 5", "expected": "0"}
            ]
        }
        response = api_admin.create_problem(problem_data, admin_cookies)
        assert response.status_code == 201
        data = response.json()
        assert "id" in data
        problem_id = data.get("id")

        detail_response = api_admin.get_problem(problem_id)
        assert detail_response.status_code == 200
        detail_data = detail_response.json()
        assert len(detail_data.get("testCases", [])) == 3


class TestNegative:
    @pytest.mark.negative
    def test_5_1_register_existing_username(self, api_regular, unique_suffix):
        username = f"testuser_{unique_suffix}"
        api_regular.register(username, "test123")
        response = api_regular.register(username, "test123")
        assert response.status_code == 400
        assert response.json().get("error") == "Username already exists"

    @pytest.mark.negative
    def test_5_2_login_wrong_password(self, api_regular, unique_suffix):
        username = f"testuser_{unique_suffix}"
        api_regular.register(username, "test123")
        response, _ = api_regular.login(username, "wrongpassword")
        assert response.status_code == 401
        assert response.json().get("error") == "Invalid username or password"

    @pytest.mark.negative
    def test_5_3_login_nonexistent_user(self, api_regular):
        response, _ = api_regular.login("nonexistent_user_12345", "password")
        assert response.status_code == 401
        assert response.json().get("error") == "Invalid username or password"

    @pytest.mark.negative
    def test_5_4_regular_user_create_problem(self, api_regular, regular_user_cookies):
        problem_data = {
            "title": "hack",
            "difficulty": "Easy",
            "content": "test",
            "template": "",
            "testCases": []
        }
        response = api_regular.create_problem(problem_data, regular_user_cookies)
        assert response.status_code == 403
        assert response.json().get("error") == "Forbidden"

    @pytest.mark.negative
    def test_5_5_regular_user_delete_problem(self, api_regular, regular_user_cookies):
        response = api_regular.delete_problem(64, regular_user_cookies)
        assert response.status_code == 403
        assert response.json().get("error") == "Forbidden"

    @pytest.mark.negative
    def test_5_6_unauthenticated_access_admin(self, api):
        problem_data = {
            "title": "hack",
            "difficulty": "Easy",
            "content": "test",
            "template": "",
            "testCases": []
        }
        response = api.create_problem(problem_data, None)
        assert response.status_code == 401
        assert response.json().get("error") == "Unauthorized"

    @pytest.mark.negative
    @pytest.mark.slow
    def test_5_7_submit_no_test_cases(self, api_admin, admin_cookies, problem_without_test_cases):
        code = "#include <iostream>"
        response = api_admin.submit_code(code, problem_without_test_cases, admin_cookies)
        assert response.status_code == 400
        assert response.json().get("error") == "No test cases configured for this problem"

    @pytest.mark.negative
    def test_5_8_submit_nonexistent_problem(self, api_admin, admin_cookies):
        code = "#include <iostream>"
        response = api_admin.submit_code(code, 99999, admin_cookies)
        assert response.status_code == 404
        assert response.json().get("error") == "Problem not found"

    @pytest.mark.negative
    def test_5_9_get_nonexistent_problem(self, api):
        response = api.get_problem(99999)
        assert response.status_code == 404
        assert response.json().get("error") == "Problem not found"

    @pytest.mark.negative
    def test_5_10_register_empty_username(self, api_regular):
        response = api_regular.register("", "password")
        assert response.status_code == 400

    @pytest.mark.negative
    def test_5_11_register_empty_password(self, api_regular):
        response = api_regular.register("testuser_empty_pwd", "")
        assert response.status_code == 400

    # ----- Auth cookie hardening (A4 / B2 / B3) -----
    # Verifies that the server emits and consumes cookies correctly:
    #   - SameSite=Lax (was Strict)
    #   - Max-Age=86400 matching the 24h server-side TTL
    #   - exact-key cookie matching (a cookie named "oj_session_legacy"
    #     must NOT be treated as the session cookie)

    @pytest.mark.negative
    def test_5_12_login_set_cookie_has_lax_and_maxage(self, api_regular, unique_suffix):
        username = f"cookie_hardening_{unique_suffix}"
        api_regular.register(username, "test123")
        response, _ = api_regular.login(username, "test123")
        assert response.status_code == 200
        # requests stores Set-Cookie on the Session. Inspect the raw header.
        sc = response.headers.get("Set-Cookie", "")
        assert "HttpOnly" in sc
        assert "SameSite=Lax" in sc
        assert "SameSite=Strict" not in sc
        assert "Max-Age=86400" in sc

    @pytest.mark.negative
    def test_5_13_me_ignores_legacy_prefix_cookie(self, api_regular, unique_suffix):
        # Manually craft a Cookie header containing a prefix-similar name.
        # /api/me should return 401 because the *exact* "oj_session" key
        # is not present.
        username = f"cookie_prefix_{unique_suffix}"
        api_regular.register(username, "test123")
        _, cookies = api_regular.login(username, "test123")
        real_token = cookies["oj_session"]
        # Build a fresh session and override its cookies with a prefix-similar
        # one. requests' jar will send both.
        legacy_session = requests.Session()
        legacy_session.cookies.set("oj_session_legacy", "fake_should_be_ignored")
        response = legacy_session.get(f"{BASE_URL}/api/me")
        assert response.status_code == 401

    @pytest.mark.negative
    def test_5_14_me_with_real_cookie_among_others(self, api_regular, unique_suffix):
        # When the *real* oj_session is present (possibly alongside others
        # including a prefix-similar one), /api/me must succeed.
        username = f"cookie_mixed_{unique_suffix}"
        api_regular.register(username, "test123")
        _, cookies = api_regular.login(username, "test123")
        real_token = cookies["oj_session"]
        s = requests.Session()
        s.cookies.set("oj_session_legacy", "should_be_ignored")
        s.cookies.set("tracker", "abc")
        s.cookies.set("oj_session", real_token)
        response = s.get(f"{BASE_URL}/api/me")
        assert response.status_code == 200
        assert response.json()["username"] == username

    @pytest.mark.negative
    def test_5_15_logout_clears_cookie_with_maxage_zero(self, api_regular, unique_suffix):
        username = f"cookie_logout_{unique_suffix}"
        api_regular.register(username, "test123")
        _, cookies = api_regular.login(username, "test123")
        response = api_regular.logout(cookies)
        assert response.status_code == 200
        sc = response.headers.get("Set-Cookie", "")
        assert "Max-Age=0" in sc
        assert "SameSite=Lax" in sc


class TestRun:
    SIMPLE_ADD_CODE = "#include <iostream>\nusing namespace std;\nint main(){int a,b;cin>>a>>b;cout<<a+b<<endl;return 0;}"
    SUBTRACT_CODE = "#include <iostream>\nint main(){int a,b;cin>>a>>b;cout<<a-b<<endl;return 0;}"
    COMPILE_ERROR_CODE = "#include <iostream>\nint main(){int x = undefined_variable; return 0;}"
    TIMEOUT_CODE = "#include <iostream>\nint main(){while(1){}return 0;}"
    SEGFAULT_CODE = "#include <iostream>\nint main(){int* p=nullptr;*p=42;return 0;}"

    @pytest.fixture
    def run_problem(self, api_admin, admin_cookies, timestamp, unique_suffix):
        problem_data = {
            "title": f"运行测试题_{timestamp}_{unique_suffix}",
            "difficulty": "Easy",
            "content": "Run handler tests",
            "template": "#include <iostream>",
            "testCases": [
                {"input": "1 2", "expected": "3\n"},
                {"input": "5 7", "expected": "12\n"},
                {"input": "10 20", "expected": "30\n"},
            ],
        }
        response = api_admin.create_problem(problem_data, admin_cookies)
        return response.json().get("id")

    @pytest.fixture
    def run_problem_no_cases(self, api_admin, admin_cookies, timestamp, unique_suffix):
        problem_data = {
            "title": f"无测试用例_运行_{timestamp}_{unique_suffix}",
            "difficulty": "Easy",
            "content": "Empty cases for run handler",
            "template": "#include <iostream>",
            "testCases": [],
        }
        response = api_admin.create_problem(problem_data, admin_cookies)
        return response.json().get("id")

    # ---- 正向：用默认用例 ----

    @pytest.mark.run
    @pytest.mark.slow
    def test_6_1_run_default_cases_all_ac(self, api_admin, admin_cookies, run_problem):
        response = api_admin.run_code(self.SIMPLE_ADD_CODE, run_problem, cases=None, cookies=admin_cookies)
        assert response.status_code == 200
        data = response.json()
        assert data["compileSuccess"] is True
        assert data["passed"] == 3
        assert data["total"] == 3
        assert data["allPassed"] is True
        assert isinstance(data["cases"], list)
        assert len(data["cases"]) == 3
        for c in data["cases"]:
            assert c["status"] == "AC"
            assert c["compileSuccess"] is True
            # AC 分支也必须回填实际输出（修复后）
            assert c["actual"] == c["expected"]

    @pytest.mark.run
    @pytest.mark.slow
    def test_6_2_run_default_cases_per_case_fields(self, api_admin, admin_cookies, run_problem):
        response = api_admin.run_code(self.SIMPLE_ADD_CODE, run_problem, cases=None, cookies=admin_cookies)
        assert response.status_code == 200
        cases = response.json()["cases"]
        for i, c in enumerate(cases):
            assert c["position"] == i
            assert "input" in c
            assert "expected" in c
            assert "actual" in c
            assert "stderr" in c
            assert "executionTimeMs" in c
            assert "compileOutput" in c
            assert "errorMessage" in c
            assert "status" in c

    @pytest.mark.run
    @pytest.mark.slow
    def test_6_3_run_default_cases_mixed_pass_fail(self, api_admin, admin_cookies, run_problem):
        # 只让 1 2 -> 3 通过；其余用减法全部 WA
        code_with_conditional = (
            "#include <iostream>\n"
            "using namespace std;\n"
            "int main(){int a,b;cin>>a>>b;"
            "if(a==1 && b==2){cout<<3<<endl;}"
            "else{cout<<a-b<<endl;}return 0;}"
        )
        response = api_admin.run_code(code_with_conditional, run_problem, cases=None, cookies=admin_cookies)
        assert response.status_code == 200
        data = response.json()
        assert data["compileSuccess"] is True
        assert data["passed"] == 1
        assert data["total"] == 3
        assert data["allPassed"] is False
        assert data["cases"][0]["status"] == "AC"
        assert data["cases"][1]["status"] == "WA"
        assert data["cases"][2]["status"] == "WA"

    # ---- 正向：用自定义用例 ----

    @pytest.mark.run
    @pytest.mark.slow
    def test_6_4_run_custom_cases_all_ac(self, api_admin, admin_cookies, run_problem):
        cases = [
            {"input": "100 200", "expected": "300\n"},
            {"input": "-5 5", "expected": "0\n"},
        ]
        response = api_admin.run_code(self.SIMPLE_ADD_CODE, run_problem, cases=cases, cookies=admin_cookies)
        assert response.status_code == 200
        data = response.json()
        assert data["compileSuccess"] is True
        assert data["passed"] == 2
        assert data["total"] == 2
        assert data["allPassed"] is True
        assert data["cases"][0]["status"] == "AC"
        assert data["cases"][0]["actual"] == "300\n"
        assert data["cases"][1]["status"] == "AC"
        assert data["cases"][1]["actual"] == "0\n"

    @pytest.mark.run
    @pytest.mark.slow
    def test_6_5_run_custom_cases_wa(self, api_admin, admin_cookies, run_problem):
        cases = [{"input": "1 2", "expected": "999\n"}]
        response = api_admin.run_code(self.SIMPLE_ADD_CODE, run_problem, cases=cases, cookies=admin_cookies)
        assert response.status_code == 200
        data = response.json()
        assert data["compileSuccess"] is True
        assert data["passed"] == 0
        assert data["total"] == 1
        assert data["allPassed"] is False
        assert data["cases"][0]["status"] == "WA"

    @pytest.mark.run
    @pytest.mark.slow
    def test_6_6_run_custom_cases_ce(self, api_admin, admin_cookies, run_problem):
        cases = [
            {"input": "1 2", "expected": "3\n"},
            {"input": "5 6", "expected": "11\n"},
        ]
        response = api_admin.run_code(self.COMPILE_ERROR_CODE, run_problem, cases=cases, cookies=admin_cookies)
        assert response.status_code == 200
        data = response.json()
        assert data["compileSuccess"] is False
        assert data["allPassed"] is False
        # CE 提前停止：只会执行首条用例
        assert data["total"] == 1
        assert data["cases"][0]["status"] == "CE"
        assert data["cases"][0]["compileSuccess"] is False
        assert "undefined_variable" in data["compileOutput"] or "undefined_variable" in data["cases"][0]["compileOutput"]

    @pytest.mark.run
    @pytest.mark.slow
    def test_6_7_run_custom_cases_tle(self, api_admin, admin_cookies, run_problem):
        cases = [{"input": "", "expected": ""}]
        response = api_admin.run_code(self.TIMEOUT_CODE, run_problem, cases=cases, cookies=admin_cookies)
        assert response.status_code == 200
        data = response.json()
        assert data["compileSuccess"] is True
        assert data["cases"][0]["status"] == "TLE"
        assert data["allPassed"] is False

    @pytest.mark.run
    @pytest.mark.slow
    def test_6_8_run_custom_cases_re(self, api_admin, admin_cookies, run_problem):
        cases = [{"input": "", "expected": ""}]
        response = api_admin.run_code(self.SEGFAULT_CODE, run_problem, cases=cases, cookies=admin_cookies)
        assert response.status_code == 200
        data = response.json()
        assert data["compileSuccess"] is True
        assert data["cases"][0]["status"] == "RE"
        assert data["allPassed"] is False

    @pytest.mark.run
    @pytest.mark.slow
    def test_6_9_run_custom_cases_position_increments(self, api_admin, admin_cookies, run_problem):
        cases = [
            {"input": "1", "expected": "1\n"},
            {"input": "2", "expected": "2\n"},
            {"input": "3", "expected": "3\n"},
        ]
        code = "#include <iostream>\nusing namespace std;\nint main(){int x;cin>>x;cout<<x<<endl;return 0;}"
        response = api_admin.run_code(code, run_problem, cases=cases, cookies=admin_cookies)
        assert response.status_code == 200
        result_cases = response.json()["cases"]
        assert [c["position"] for c in result_cases] == [0, 1, 2]

    @pytest.mark.run
    def test_6_10_run_custom_cases_does_not_require_problem_id(self, api_admin, admin_cookies):
        # 自定义用例模式下，problemId 任意均可（不查库）
        cases = [{"input": "1 2", "expected": "3\n"}]
        response = api_admin.run_code(self.SIMPLE_ADD_CODE, 99999, cases=cases, cookies=admin_cookies)
        assert response.status_code == 200
        data = response.json()
        assert data["allPassed"] is True
        assert data["passed"] == 1

    # ---- 负面场景 ----

    @pytest.mark.run
    @pytest.mark.negative
    def test_6_11_run_invalid_json(self, api_admin, admin_cookies):
        # 发送一个无法被解析为 JSON 的原始字节体
        response = api_admin.session.post(
            f"{api_admin.base_url}/api/run",
            data=b"this is not valid json {[",
            headers={"Content-Type": "application/json"},
            cookies=admin_cookies,
        )
        assert response.status_code == 400
        assert response.json().get("error") == "Invalid JSON"

    @pytest.mark.run
    @pytest.mark.negative
    def test_6_12_run_missing_code(self, api_admin, admin_cookies):
        response = api_admin.post("/api/run", json_data={"problemId": 1}, cookies=admin_cookies)
        assert response.status_code == 400
        assert response.json().get("error") == "Missing required fields: code, problemId"

    @pytest.mark.run
    @pytest.mark.negative
    def test_6_13_run_missing_problem_id(self, api_admin, admin_cookies):
        response = api_admin.post("/api/run", json_data={"code": "int main(){return 0;}"}, cookies=admin_cookies)
        assert response.status_code == 400
        assert response.json().get("error") == "Missing required fields: code, problemId"

    @pytest.mark.run
    @pytest.mark.negative
    def test_6_14_run_empty_code(self, api_admin, admin_cookies, run_problem):
        response = api_admin.run_code("", run_problem, cases=None, cookies=admin_cookies)
        assert response.status_code == 400
        assert response.json().get("error") == "Code cannot be empty"

    @pytest.mark.run
    @pytest.mark.negative
    def test_6_15_run_default_cases_problem_not_found(self, api_admin, admin_cookies):
        # 不传 cases 时，不存在的 problemId 应返回 404
        response = api_admin.run_code(self.SIMPLE_ADD_CODE, 99999, cases=None, cookies=admin_cookies)
        assert response.status_code == 404
        assert response.json().get("error") == "Problem not found"

    @pytest.mark.run
    @pytest.mark.negative
    def test_6_16_run_default_cases_problem_has_no_cases(self, api_admin, admin_cookies, run_problem_no_cases):
        response = api_admin.run_code(self.SIMPLE_ADD_CODE, run_problem_no_cases, cases=None, cookies=admin_cookies)
        assert response.status_code == 400
        assert response.json().get("error") == "No test cases configured for this problem"

    @pytest.mark.run
    @pytest.mark.negative
    def test_6_17_run_empty_custom_cases_array(self, api_admin, admin_cookies, run_problem):
        response = api_admin.run_code(self.SIMPLE_ADD_CODE, run_problem, cases=[], cookies=admin_cookies)
        assert response.status_code == 400
        assert response.json().get("error") == "请添加至少一个测试用例"