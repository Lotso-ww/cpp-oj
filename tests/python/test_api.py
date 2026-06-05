import pytest


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