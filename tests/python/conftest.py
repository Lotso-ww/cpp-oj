import pytest
import time
import random
import requests
import logging
import os
import subprocess
from typing import Dict, Optional, List

BASE_URL = os.environ.get("API_BASE_URL", "http://localhost:8080")
CONTENT_TYPE = "application/json"
TIMEOUT = 30

logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s - %(levelname)s - %(message)s",
    handlers=[
        logging.FileHandler(f"/tmp/pytest_api_{int(time.time())}.log"),
        logging.StreamHandler()
    ]
)
logger = logging.getLogger(__name__)


def is_server_running(url: str, timeout: float = 2.0) -> bool:
    try:
        response = requests.get(f"{url}/api/problems", timeout=timeout)
        return response.status_code == 200
    except requests.exceptions.RequestException:
        return False


@pytest.fixture(scope="session", autouse=True)
def check_server():
    if not is_server_running(BASE_URL):
        pytest.exit(f"服务器未启动或无法访问: {BASE_URL}\n请先启动服务器再运行测试", returncode=1)
    logger.info(f"服务器连接检查通过: {BASE_URL}")


@pytest.fixture(scope="session")
def api():
    return APIClient(base_url=BASE_URL)


@pytest.fixture(scope="session")
def api_admin():
    return APIClient(base_url=BASE_URL)


@pytest.fixture
def api_regular():
    return APIClient(base_url=BASE_URL)


@pytest.fixture(scope="session")
def timestamp():
    return int(time.time())


@pytest.fixture(scope="session")
def unique_suffix(timestamp):
    random.seed(timestamp)
    return f"{timestamp}_{random.randint(1000, 9999)}"


@pytest.fixture(scope="session")
def admin_cookies(api_admin):
    logger.info("Admin login (session scope)")
    _, cookies = api_admin.login("admin", "admin123")
    return cookies


@pytest.fixture
def regular_user_cookies(api_regular, unique_suffix):
    username = f"testuser_{unique_suffix}_{random.randint(1000, 9999)}"
    api_regular.register(username, "test123")
    _, cookies = api_regular.login(username, "test123")
    return cookies


@pytest.fixture
def problem_without_test_cases(api_admin, admin_cookies, timestamp, unique_suffix):
    problem_data = {
        "title": f"无测试用例题目_{timestamp}_{unique_suffix}",
        "difficulty": "Easy",
        "content": "test",
        "template": "#include <iostream>",
        "testCases": []
    }
    response = api_admin.create_problem(problem_data, admin_cookies)
    return response.json().get("id")


@pytest.fixture(scope="function", autouse=True)
def cleanup_created_resources(request, api_admin, admin_cookies):
    created_problem_ids: List[int] = []

    def track_problem(problem_id: int):
        created_problem_ids.append(problem_id)

    yield track_problem

    for pid in created_problem_ids:
        try:
            api_admin.delete_problem(pid, admin_cookies)
            logger.info(f"Cleaned up problem: {pid}")
        except Exception as e:
            logger.warning(f"Failed to clean up problem {pid}: {e}")


class APIClient:
    def __init__(self, base_url: str = BASE_URL):
        self.base_url = base_url
        self.session = requests.Session()
        self.session.timeout = TIMEOUT

    def post(self, endpoint: str, json_data: Optional[Dict] = None, cookies: Optional[Dict] = None) -> requests.Response:
        url = f"{self.base_url}{endpoint}"
        headers = {"Content-Type": CONTENT_TYPE}
        response = self.session.post(url, json=json_data, cookies=cookies, headers=headers)
        logger.info(f"POST {endpoint} -> {response.status_code}")
        return response

    def get(self, endpoint: str, cookies: Optional[Dict] = None) -> requests.Response:
        url = f"{self.base_url}{endpoint}"
        response = self.session.get(url, cookies=cookies)
        logger.info(f"GET {endpoint} -> {response.status_code}")
        return response

    def delete(self, endpoint: str, cookies: Optional[Dict] = None) -> requests.Response:
        url = f"{self.base_url}{endpoint}"
        response = self.session.delete(url, cookies=cookies)
        logger.info(f"DELETE {endpoint} -> {response.status_code}")
        return response

    def register(self, username: str, password: str) -> requests.Response:
        return self.post("/api/register", json_data={"username": username, "password": password})

    def login(self, username: str, password: str) -> tuple:
        response = self.post("/api/login", json_data={"username": username, "password": password})
        return response, self.session.cookies.get_dict()

    def logout(self, cookies: Optional[Dict] = None) -> requests.Response:
        return self.post("/api/logout", cookies=cookies)

    def get_problems(self) -> requests.Response:
        return self.get("/api/problems")

    def get_problem(self, problem_id: int) -> requests.Response:
        return self.get(f"/api/problems/{problem_id}")

    def submit_code(self, code: str, problem_id: int, cookies: Optional[Dict] = None) -> requests.Response:
        return self.post("/api/submit", json_data={"code": code, "problemId": problem_id}, cookies=cookies)

    def run_code(self, code: str, problem_id: int, cases: Optional[List[Dict]] = None, cookies: Optional[Dict] = None) -> requests.Response:
        json_data = {"code": code, "problemId": problem_id}
        if cases is not None:
            json_data["cases"] = cases
        return self.post("/api/run", json_data=json_data, cookies=cookies)

    def create_problem(self, problem_data: Dict, cookies: Optional[Dict] = None) -> requests.Response:
        return self.post("/api/admin/problems", json_data=problem_data, cookies=cookies)

    def delete_problem(self, problem_id: int, cookies: Optional[Dict] = None) -> requests.Response:
        return self.delete(f"/api/admin/problems/{problem_id}", cookies=cookies)