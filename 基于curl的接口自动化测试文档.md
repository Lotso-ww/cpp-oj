# 基于 curl 的接口自动化测试文档

## 环境说明

| 项目 | 说明 |
|------|------|
| **Base URL** | `http://localhost:8080` |
| **Content-Type** | `application/json` |
| **Cookie 文件** | `/tmp/cookies.txt` |

---

## 1. 认证模块

### 1.1 用户注册

```bash
curl -s -w "\nHTTP_CODE:%{http_code}" -X POST http://localhost:8080/api/register \
  -H "Content-Type: application/json" \
  -d '{"username":"testuser","password":"test123"}'
```

**预期响应：** `{"message":"User registered successfully"}` (201)

---

### 1.2 用户登录

```bash
curl -s -w "\nHTTP_CODE:%{http_code}" -X POST http://localhost:8080/api/login \
  -H "Content-Type: application/json" \
  -d '{"username":"testuser","password":"test123"}' \
  -c /tmp/cookies.txt
```

**预期响应：** `{"message":"Login successful"}` (200)

---

### 1.3 用户登出

```bash
curl -s -w "\nHTTP_CODE:%{http_code}" -X POST http://localhost:8080/api/logout \
  -b /tmp/cookies.txt
```

**预期响应：** `{"message":"Logout successful"}` (200)

---

### 1.4 管理员登录

```bash
curl -s -w "\nHTTP_CODE:%{http_code}" -X POST http://localhost:8080/api/login \
  -H "Content-Type: application/json" \
  -d '{"username":"admin","password":"admin123"}' \
  -c /tmp/admin_cookies.txt
```

**预期响应：** `{"message":"Login successful"}` (200)

---

## 2. 题目模块

### 2.1 获取题目列表

```bash
curl -s -w "\nHTTP_CODE:%{http_code}" http://localhost:8080/api/problems
```

**预期响应：** JSON 数组包含 problems 和 total 字段 (200)

---

### 2.2 获取题目详情

```bash
curl -s -w "\nHTTP_CODE:%{http_code}" http://localhost:8080/api/problems/64
```

**预期响应：** JSON 包含 id, title, difficulty, content, template, testCases (200)

---

## 3. 提交模块

### 3.1 提交代码执行（AC）

```bash
curl -s -w "\nHTTP_CODE:%{http_code}" -X POST http://localhost:8080/api/submit \
  -H "Content-Type: application/json" \
  -d '{"code":"#include <iostream>\nusing namespace std;\nint main() { int a,b; cin>>a>>b; cout<<a+b; return 0; }","problemId":840}' \
  -b /tmp/admin_cookies.txt
```

**预期响应 (AC)：** `{"status":"AC","stdout":"...","executionTimeMs":...}` (200)

---

### 3.2 提交代码执行（CE）

```bash
curl -s -w "\nHTTP_CODE:%{http_code}" -X POST http://localhost:8080/api/submit \
  -H "Content-Type: application/json" \
  -d '{"code":"#include <iostream>\nint main() { int x = undefined_variable; return 0; }","problemId":840}' \
  -b /tmp/admin_cookies.txt
```

**预期响应 (CE)：** `{"status":"CE","compileOutput":"...undefined_variable was not declared...","error":"Compilation failed"}` (200)

---

## 4. 管理模块

### 4.1 新增题目（管理员）

```bash
curl -s -w "\nHTTP_CODE:%{http_code}" -X POST http://localhost:8080/api/admin/problems \
  -H "Content-Type: application/json" \
  -d '{"title":"测试题","difficulty":"Easy","content":"测试内容","template":"#include <iostream>","testCases":[{"input":"1 2","expected":"3"}]}' \
  -b /tmp/admin_cookies.txt
```

**预期响应：** `{"id":...,"title":"测试题"}` (201)

---

### 4.2 删除题目（管理员）

```bash
curl -s -w "\nHTTP_CODE:%{http_code}" -X DELETE http://localhost:8080/api/admin/problems/840 \
  -b /tmp/admin_cookies.txt
```

**预期响应：** `{"message":"Problem deleted successfully"}` (200)

---

### 4.3 删除不存在的题目

```bash
curl -s -w "\nHTTP_CODE:%{http_code}" -X DELETE http://localhost:8080/api/admin/problems/99999 \
  -b /tmp/admin_cookies.txt
```

**预期响应：** `{"error":"Problem not found"}` (404)

---

## 5. 负面测试场景

### 5.1 注册已存在的用户名

```bash
curl -s -w "\nHTTP_CODE:%{http_code}" -X POST http://localhost:8080/api/register \
  -H "Content-Type: application/json" \
  -d '{"username":"testuser","password":"test123"}'
```

**预期响应：** `{"error":"Username already exists"}` (400)

---

### 5.2 登录失败（错误密码）

```bash
curl -s -w "\nHTTP_CODE:%{http_code}" -X POST http://localhost:8080/api/login \
  -H "Content-Type: application/json" \
  -d '{"username":"testuser","password":"wrongpassword"}'
```

**预期响应：** `{"error":"Invalid username or password"}` (401)

---

### 5.3 普通用户调用管理员接口（创建题目）

```bash
# 先用普通用户登录
curl -s -X POST http://localhost:8080/api/login \
  -H "Content-Type: application/json" \
  -d '{"username":"testuser","password":"test123"}' \
  -c /tmp/regular_cookies.txt

# 普通用户尝试创建题目
curl -s -w "\nHTTP_CODE:%{http_code}" -X POST http://localhost:8080/api/admin/problems \
  -H "Content-Type: application/json" \
  -d '{"title":"hack","difficulty":"Easy","content":"test","template":"","testCases":[]}' \
  -b /tmp/regular_cookies.txt
```

**预期响应：** `{"error":"Forbidden"}` (403)

---

### 5.4 普通用户调用管理员接口（删除题目）

```bash
curl -s -w "\nHTTP_CODE:%{http_code}" -X DELETE http://localhost:8080/api/admin/problems/64 \
  -b /tmp/regular_cookies.txt
```

**预期响应：** `{"error":"Forbidden"}` (403)

---

### 5.5 未登录用户访问需认证接口

```bash
curl -s -w "\nHTTP_CODE:%{http_code}" -X POST http://localhost:8080/api/admin/problems \
  -H "Content-Type: application/json" \
  -d '{"title":"hack","difficulty":"Easy","content":"test","template":"","testCases":[]}'
```

**预期响应：** `{"error":"Unauthorized"}` (401)

---

### 5.6 提交代码到无测试用例的题目

```bash
curl -s -w "\nHTTP_CODE:%{http_code}" -X POST http://localhost:8080/api/submit \
  -H "Content-Type: application/json" \
  -d '{"code":"#include <iostream>","problemId":64}' \
  -b /tmp/admin_cookies.txt
```

**预期响应：** `{"error":"No test cases configured for this problem"}` (400)

---

### 5.7 提交代码到不存在的题目

```bash
curl -s -w "\nHTTP_CODE:%{http_code}" -X POST http://localhost:8080/api/submit \
  -H "Content-Type: application/json" \
  -d '{"code":"#include <iostream>","problemId":99999}' \
  -b /tmp/admin_cookies.txt
```

**预期响应：** `{"error":"Problem not found"}` (404)

---

### 5.8 获取不存在的题目详情

```bash
curl -s -w "\nHTTP_CODE:%{http_code}" http://localhost:8080/api/problems/99999
```

**预期响应：** `{"error":"Problem not found"}` (404)

---

## 6. 一键自动化测试脚本

```bash
#!/bin/bash

BASE_URL="http://localhost:8080"
REGULAR_COOKIE="/tmp/regular_cookies.txt"
ADMIN_COOKIE="/tmp/admin_cookies.txt"
LOG_FILE="/tmp/test_results_$(date +%Y%m%d_%H%M%S).log"

echo "========== 接口自动化测试 ==========" | tee $LOG_FILE

# 1. 用户注册（使用时间戳+随机数确保唯一）
echo -e "\n[1.1] 用户注册" | tee -a $LOG_FILE
UNIQUE_USER="testuser_$(date +%s)_$RANDOM"
REGISTER_RESULT=$(curl -s -w "\nHTTP_CODE:%{http_code}" -X POST $BASE_URL/api/register \
  -H "Content-Type: application/json" \
  -d "{\"username\":\"$UNIQUE_USER\",\"password\":\"test123\"}")
echo "$REGISTER_RESULT" | tee -a $LOG_FILE

# 2. 普通用户登录
echo -e "\n\n[1.2] 普通用户登录" | tee -a $LOG_FILE
curl -s -X POST $BASE_URL/api/login \
  -H "Content-Type: application/json" \
  -d '{"username":"testuser","password":"test123"}' \
  -c $REGULAR_COOKIE | tee -a $LOG_FILE

# 3. 管理员登录
echo -e "\n\n[1.4] 管理员登录" | tee -a $LOG_FILE
curl -s -X POST $BASE_URL/api/login \
  -H "Content-Type: application/json" \
  -d '{"username":"admin","password":"admin123"}' \
  -c $ADMIN_COOKIE | tee -a $LOG_FILE

# 4. 管理员创建题目（用于后续测试）
echo -e "\n\n[4.1] 管理员创建题目" | tee -a $LOG_FILE
CREATE_RESULT=$(curl -s -X POST $BASE_URL/api/admin/problems \
  -H "Content-Type: application/json" \
  -d '{"title":"自动化测试题","difficulty":"Easy","content":"计算a+b","template":"#include <iostream>","testCases":[{"input":"1 2","expected":"3"}]}' \
  -b $ADMIN_COOKIE)
echo "$CREATE_RESULT" | tee -a $LOG_FILE
PROBLEM_ID=$(echo "$CREATE_RESULT" | grep -oE '"id":[[:space:]]*[0-9]+' | grep -oE '[0-9]+')

# 5. 获取题目详情
echo -e "\n\n[2.2] 获取题目详情" | tee -a $LOG_FILE
curl -s "$BASE_URL/api/problems/$PROBLEM_ID" | tee -a $LOG_FILE
echo ""

# 6. 提交代码（正常AC）
echo -e "\n\n[3.1] 提交代码(AC)" | tee -a $LOG_FILE
SUBMIT_AC_RESULT=$(curl -s -w "\nHTTP_CODE:%{http_code}" -X POST $BASE_URL/api/submit \
  -H "Content-Type: application/json" \
  -d "{\"code\":\"#include <iostream>\\nusing namespace std;\\nint main() { int a,b; cin>>a>>b; cout<<a+b; return 0; }\",\"problemId\":$PROBLEM_ID}" \
  -b $ADMIN_COOKIE)
echo "$SUBMIT_AC_RESULT" | tee -a $LOG_FILE

# 7. 提交代码（CE）
echo -e "\n\n[3.2] 提交代码(CE)" | tee -a $LOG_FILE
SUBMIT_CE_RESULT=$(curl -s -w "\nHTTP_CODE:%{http_code}" -X POST $BASE_URL/api/submit \
  -H "Content-Type: application/json" \
  -d "{\"code\":\"#include <iostream>\\nint main() { int x = undefined_variable; return 0; }\",\"problemId\":$PROBLEM_ID}" \
  -b $ADMIN_COOKIE)
echo "$SUBMIT_CE_RESULT" | tee -a $LOG_FILE

# 8. 提交到不存在的题目
echo -e "\n\n[5.7] 提交到不存在的题目" | tee -a $LOG_FILE
curl -s -w "\nHTTP_CODE:%{http_code}" -X POST $BASE_URL/api/submit \
  -H "Content-Type: application/json" \
  -d '{"code":"#include <iostream>","problemId":99999}' \
  -b $ADMIN_COOKIE | tee -a $LOG_FILE
echo ""

# 9. 获取不存在的题目详情
echo -e "\n\n[5.8] 获取不存在的题目详情" | tee -a $LOG_FILE
curl -s -w "\nHTTP_CODE:%{http_code}" "$BASE_URL/api/problems/99999" | tee -a $LOG_FILE
echo ""

# 10. 普通用户尝试创建题目（应被拒绝）
echo -e "\n\n[5.3] 普通用户创建题目（应被拒绝）" | tee -a $LOG_FILE
curl -s -w "\nHTTP_CODE:%{http_code}" -X POST $BASE_URL/api/admin/problems \
  -H "Content-Type: application/json" \
  -d '{"title":"hack","difficulty":"Easy","content":"test","template":"","testCases":[]}' \
  -b $REGULAR_COOKIE | tee -a $LOG_FILE
echo ""

# 11. 普通用户尝试删除题目（应被拒绝）
echo -e "\n\n[5.4] 普通用户删除题目（应被拒绝）" | tee -a $LOG_FILE
curl -s -w "\nHTTP_CODE:%{http_code}" -X DELETE $BASE_URL/api/admin/problems/64 \
  -b $REGULAR_COOKIE | tee -a $LOG_FILE
echo ""

# 12. 管理员删除题目
echo -e "\n\n[4.2] 管理员删除题目" | tee -a $LOG_FILE
curl -s -X DELETE $BASE_URL/api/admin/problems/$PROBLEM_ID \
  -b $ADMIN_COOKIE | tee -a $LOG_FILE
echo ""

# 13. 管理员删除不存在的题目（404）
echo -e "\n\n[4.3] 删除不存在的题目" | tee -a $LOG_FILE
curl -s -w "\nHTTP_CODE:%{http_code}" -X DELETE $BASE_URL/api/admin/problems/99999 \
  -b $ADMIN_COOKIE | tee -a $LOG_FILE
echo ""

# 14. 用户登出
echo -e "\n\n[1.3] 用户登出" | tee -a $LOG_FILE
curl -s -X POST $BASE_URL/api/logout \
  -b $REGULAR_COOKIE | tee -a $LOG_FILE
echo ""

echo -e "\n\n========== 测试完成 ==========" | tee -a $LOG_FILE
echo "详细日志已保存到: $LOG_FILE"

```bash
#!/bin/bash

BASE_URL="http://localhost:8080"
REGULAR_COOKIE="/tmp/regular_cookies.txt"
ADMIN_COOKIE="/tmp/admin_cookies.txt"

echo "========== 接口自动化测试 =========="

# 1. 用户注册
echo -e "\n[1.1] 用户注册"
REGISTER_RESULT=$(curl -s -w "\nHTTP_CODE:%{http_code}" -X POST $BASE_URL/api/register \
  -H "Content-Type: application/json" \
  -d '{"username":"newuser_'$(date +%s)'","password":"test123"}')
echo "$REGISTER_RESULT"

# 2. 普通用户登录
echo -e "\n\n[1.2] 普通用户登录"
curl -s -X POST $BASE_URL/api/login \
  -H "Content-Type: application/json" \
  -d '{"username":"testuser","password":"test123"}' \
  -c $REGULAR_COOKIE
echo ""

# 3. 管理员登录
echo -e "\n\n[1.4] 管理员登录"
curl -s -X POST $BASE_URL/api/login \
  -H "Content-Type: application/json" \
  -d '{"username":"admin","password":"admin123"}' \
  -c $ADMIN_COOKIE
echo ""

# 4. 管理员创建题目（用于后续测试）
echo -e "\n\n[4.1] 管理员创建题目"
CREATE_RESULT=$(curl -s -X POST $BASE_URL/api/admin/problems \
  -H "Content-Type: application/json" \
  -d '{"title":"自动化测试题","difficulty":"Easy","content":"计算a+b","template":"#include <iostream>","testCases":[{"input":"1 2","expected":"3"}]}' \
  -b $ADMIN_COOKIE)
echo "$CREATE_RESULT"
PROBLEM_ID=$(echo "$CREATE_RESULT" | grep -o '"id":[0-9]*' | cut -d':' -f2)

# 5. 提交代码（正常AC）
echo -e "\n\n[3.1] 提交代码(AC)"
curl -s -X POST $BASE_URL/api/submit \
  -H "Content-Type: application/json" \
  -d '{"code":"#include <iostream>\nusing namespace std;\nint main() { int a,b; cin>>a>>b; cout<<a+b; return 0; }","problemId":'$PROBLEM_ID'}' \
  -b $ADMIN_COOKIE
echo ""

# 6. 提交代码（CE）
echo -e "\n\n[3.2] 提交代码(CE)"
curl -s -X POST $BASE_URL/api/submit \
  -H "Content-Type: application/json" \
  -d '{"code":"int main() { return 0 }","problemId":'$PROBLEM_ID'}' \
  -b $ADMIN_COOKIE
echo ""

# 7. 普通用户尝试创建题目（应被拒绝）
echo -e "\n\n[5.3] 普通用户创建题目（应被拒绝）"
curl -s -w "\nHTTP_CODE:%{http_code}" -X POST $BASE_URL/api/admin/problems \
  -H "Content-Type: application/json" \
  -d '{"title":"hack","difficulty":"Easy","content":"test","template":"","testCases":[]}' \
  -b $REGULAR_COOKIE
echo ""

# 8. 普通用户尝试删除题目（应被拒绝）
echo -e "\n\n[5.4] 普通用户删除题目（应被拒绝）"
curl -s -w "\nHTTP_CODE:%{http_code}" -X DELETE $BASE_URL/api/admin/problems/64 \
  -b $REGULAR_COOKIE
echo ""

# 9. 管理员删除题目
echo -e "\n\n[4.2] 管理员删除题目"
curl -s -X DELETE $BASE_URL/api/admin/problems/$PROBLEM_ID \
  -b $ADMIN_COOKIE
echo ""

# 10. 管理员删除不存在的题目（404）
echo -e "\n\n[4.3] 删除不存在的题目"
curl -s -w "\nHTTP_CODE:%{http_code}" -X DELETE $BASE_URL/api/admin/problems/99999 \
  -b $ADMIN_COOKIE
echo ""

# 11. 用户登出
echo -e "\n\n[1.3] 用户登出"
curl -s -X POST $BASE_URL/api/logout \
  -b $REGULAR_COOKIE
echo ""

echo -e "\n\n========== 测试完成 =========="
```

---

## 7. 测试结果汇总

### 正面测试

| 序号 | 接口 | 方法 | 状态码 | 结果 |
|------|------|------|--------|------|
| 1 | `/api/register` | POST | 201 | ✅ |
| 2 | `/api/login` | POST | 200 | ✅ |
| 3 | `/api/logout` | POST | 200 | ✅ |
| 4 | `/api/problems` | GET | 200 | ✅ |
| 5 | `/api/problems/:id` | GET | 200 | ✅ |
| 6 | `/api/submit` (AC) | POST | 200 | ✅ |
| 7 | `/api/submit` (CE) | POST | 200 | ✅ |
| 8 | `/api/admin/problems` | POST | 201 | ✅ |
| 9 | `/api/admin/problems/:id` | DELETE | 200 | ✅ |

### 负面测试

| 序号 | 测试场景 | 接口 | 状态码 | 结果 |
|------|------|------|--------|------|
| 1 | 注册已存在用户名 | `/api/register` | 400 | ✅ |
| 2 | 登录失败（错误密码） | `/api/login` | 401 | ✅ |
| 3 | 普通用户创建题目 | `/api/admin/problems` | 403 | ✅ |
| 4 | 普通用户删除题目 | `/api/admin/problems/:id` | 403 | ✅ |
| 5 | 未登录访问管理员接口 | `/api/admin/problems` | 401 | ✅ |
| 6 | 删除不存在的题目 | `/api/admin/problems/:id` | 404 | ✅ |
| 7 | 提交到无测试用例题目 | `/api/submit` | 400 | ✅ |
| 8 | 提交到不存在的题目 | `/api/submit` | 404 | ✅ |
| 9 | 获取不存在的题目详情 | `/api/problems/:id` | 404 | ✅ |

---

## 8. 注意事项

1. 测试前请确保服务器已启动 (`http://localhost:8080` 可访问)
2. 管理模块接口需要先登录获取 cookie 并通过 `-b` 参数传递
3. 提交代码接口需要有效的 problemId（数据库中已配置测试用例的题目）
4. 测试脚本会自动保存 cookie 到 `/tmp/cookies.txt`
5. 负面测试同样重要，可以发现权限漏洞、边界条件等问题
6. 测试日志会保存到 `/tmp/test_results_YYYYMMDD_HHMMSS.log`