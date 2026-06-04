# OJ System API Documentation

## 全局说明

| 项目 | 说明 |
|------|------|
| **Base URL** | `http://{host}:{port}` (默认 `http://localhost:8080`) |
| **Content-Type** | `application/json` |
| **字符编码** | UTF-8 |
| **认证方式** | Session/Cookie (`Cookie: oj_session=<session_token>`) |
| **状态码规范** | 200 成功, 201 创建成功, 400 请求错误, 401 未认证, 404 未找到, 500 服务器错误 |

---

## 目录

- [认证模块](#认证模块)
- [题目模块](#题目模块)
- [提交模块](#提交模块)
- [管理模块](#管理模块)

---

## 认证模块

### 1.1 用户登录

**接口名称及作用：** 用户登录，验证用户名和密码成功后建立会话。

**请求路径 (Path)：** `/api/login`

**请求方式 (Method)：** `POST`

**请求参数 (Parameters)：**

| 位置 | 字段名 | 类型 | 是否必填 | 说明 |
|------|--------|------|----------|------|
| Body | username | string | 是 | 用户名，长度 3-64 字符 |
| Body | password | string | 是 | 密码，最小 6 字符 |

**请求示例 (JSON)：**
```json
{
    "username": "alice",
    "password": "secret123"
}
```

**响应结果 (Response)：**

成功响应 (200 OK)：
```json
{
    "message": "Login successful"
}
```

| 字段名 | 类型 | 说明 |
|--------|------|------|
| message | string | 成功消息 |

失败响应：

| 状态码 | 字段名 | 说明 |
|--------|--------|------|
| 400 | error | Invalid JSON / Missing username or password |
| 401 | error | Invalid username or password |

---

### 1.2 用户登出

**接口名称及作用：** 清除当前会话，注销登录状态。

**请求路径 (Path)：** `/api/logout`

**请求方式 (Method)：** `POST`

**请求参数 (Parameters)：**

| 位置 | 字段名 | 类型 | 是否必填 | 说明 |
|------|--------|------|----------|------|
| Header | Cookie | string | 否 | 当前会话 cookie（可选） |

**响应结果 (Response)：**

成功响应 (200 OK)：
```json
{
    "message": "Logout successful"
}
```

| 字段名 | 类型 | 说明 |
|--------|------|------|
| message | string | 成功消息 |

---

### 1.3 用户注册

**接口名称及作用：** 注册新用户，用户名唯一性校验。

**请求路径 (Path)：** `/api/register`

**请求方式 (Method)：** `POST`

**请求参数 (Parameters)：**

| 位置 | 字段名 | 类型 | 是否必填 | 说明 |
|------|--------|------|----------|------|
| Body | username | string | 是 | 用户名，长度 3-64 字符 |
| Body | password | string | 是 | 密码，最小 6 字符 |

**请求示例 (JSON)：**
```json
{
    "username": "bob",
    "password": "password123"
}
```

**响应结果 (Response)：**

成功响应 (201 Created)：
```json
{
    "message": "User registered successfully"
}
```

| 字段名 | 类型 | 说明 |
|--------|------|------|
| message | string | 成功消息 |

失败响应：

| 状态码 | 字段名 | 说明 |
|--------|--------|------|
| 400 | error | Invalid JSON |
| 400 | error | Missing username or password |
| 400 | error | Username must be between 3 and 64 characters |
| 400 | error | Password must be at least 6 characters |
| 400 | error | Username already exists |

---

## 题目模块

### 2.1 获取题目列表

**接口名称及作用：** 获取所有题目的概要列表（编号、标题、难度）。

**请求路径 (Path)：** `/api/problems`

**请求方式 (Method)：** `GET`

**请求参数 (Parameters)：** 无

**响应结果 (Response)：**

成功响应 (200 OK)：
```json
{
    "problems": [
        {
            "id": 1,
            "title": "两数之和",
            "difficulty": "Easy"
        },
        {
            "id": 2,
            "title": "反转链表",
            "difficulty": "Medium"
        }
    ],
    "total": 2
}
```

| 字段名 | 类型 | 说明 |
|--------|------|------|
| problems | array | 题目列表 |
| problems[].id | int | 题目 ID |
| problems[].title | string | 题目标题 |
| problems[].difficulty | string | 难度级别：Easy / Medium / Hard |
| total | int | 题目总数 |

---

### 2.2 获取题目详情

**接口名称及作用：** 获取指定题目的完整信息，包含题目描述、代码模板和测试用例。

**请求路径 (Path)：** `/api/problems/:id`

**请求方式 (Method)：** `GET`

**请求参数 (Parameters)：**

| 位置 | 字段名 | 类型 | 是否必填 | 说明 |
|------|--------|------|----------|------|
| Path | id | int | 是 | 题目 ID（正整数） |

**响应结果 (Response)：**

成功响应 (200 OK)：
```json
{
    "id": 1,
    "title": "两数之和",
    "difficulty": "Easy",
    "content": "给定一个整数数组 nums 和一个整数目标值 target...",
    "template": "#include <iostream>\nusing namespace std;\n\nint main() {\n    // your code here\n    return 0;\n}",
    "testCases": [
        {
            "id": 1,
            "input": "2 7 11 15\n9",
            "expected": "0 1",
            "position": 0
        },
        {
            "id": 2,
            "input": "3 2 4 3\n6",
            "expected": "1 2",
            "position": 1
        }
    ]
}
```

| 字段名 | 类型 | 说明 |
|--------|------|------|
| id | int | 题目 ID |
| title | string | 题目标题 |
| difficulty | string | 难度级别：Easy / Medium / Hard |
| content | string | 题目描述（Markdown 格式） |
| template | string | 代码模板（C++） |
| testCases | array | 测试用例列表 |
| testCases[].id | int | 用例 ID |
| testCases[].input | string | 输入数据 |
| testCases[].expected | string | 期望输出 |
| testCases[].position | int | 排序序号 |

失败响应：

| 状态码 | 字段名 | 说明 |
|--------|--------|------|
| 400 | error | Invalid problem ID |
| 404 | error | Problem not found |

---

## 提交模块

### 3.1 提交代码执行

**接口名称及作用：** 提交 C++ 代码到指定题目，系统编译并运行代码，使用测试用例验证结果。

**请求路径 (Path)：** `/api/submit`

**请求方式 (Method)：** `POST`

**请求参数 (Parameters)：**

| 位置 | 字段名 | 类型 | 是否必填 | 说明 |
|------|--------|------|----------|------|
| Body | code | string | 是 | C++ 源代码 |
| Body | problemId | int | 是 | 题目 ID |

**请求示例 (JSON)：**
```json
{
    "code": "#include <iostream>\nusing namespace std;\n\nint main() {\n    int n;\n    cin >> n;\n    cout << n * 2 << endl;\n    return 0;\n}",
    "problemId": 1
}
```

**响应结果 (Response)：**

AC - 全部通过 (200 OK)：
```json
{
    "status": "AC",
    "stdout": "0 1",
    "executionTimeMs": 12
}
```

CE - 编译错误 (200 OK)：
```json
{
    "status": "CE",
    "compileOutput": "error: 'cin' undeclared identifier\n",
    "error": "Compilation failed"
}
```

RE - 运行时错误 (200 OK)：
```json
{
    "status": "RE",
    "stderr": "Segmentation fault",
    "stdout": "",
    "executionTimeMs": 5,
    "error": "Runtime error"
}
```

TLE - 超时 (200 OK)：
```json
{
    "status": "TLE"
}
```

MLE - 内存超限 (200 OK)：
```json
{
    "status": "MLE"
}
```

| 字段名 | 类型 | 说明 |
|--------|------|------|
| status | string | 结果状态码 |
| stdout | string | 标准输出 |
| executionTimeMs | int | 执行耗时（毫秒） |
| compileOutput | string | 编译错误输出 |
| stderr | string | 标准错误输出 |
| error | string | 错误描述 |

**状态码 (status) 说明：**

| 状态码 | 含义 |
|--------|------|
| AC | Accepted - 所有测试用例通过 |
| CE | Compilation Error - 编译失败 |
| RE | Runtime Error - 运行时错误（如段错误、除零） |
| TLE | Time Limit Exceeded - 执行超时（默认 5 秒） |
| MLE | Memory Limit Exceeded - 内存超限 |
| SYSTEM_ERROR | 系统内部错误 |

失败响应：

| 状态码 | 字段名 | 说明 |
|--------|--------|------|
| 400 | error | Invalid JSON |
| 400 | error | Missing required fields: code, problemId |
| 400 | error | Code cannot be empty |
| 400 | error | No test cases configured for this problem |
| 404 | error | Problem not found |

---

## 管理模块

### 4.1 新增题目

**接口名称及作用：** 管理员新增一道题目及其测试用例。

**请求路径 (Path)：** `/api/admin/problems`

**请求方式 (Method)：** `POST`

**请求参数 (Parameters)：**

| 位置 | 字段名 | 类型 | 是否必填 | 说明 |
|------|--------|------|----------|------|
| Body | title | string | 是 | 题目标题 |
| Body | difficulty | string | 是 | 难度：Easy / Medium / Hard |
| Body | content | string | 是 | 题目描述（Markdown） |
| Body | template | string | 否 | 代码模板（C++） |
| Body | testCases | array | 否 | 测试用例列表 |

**请求示例 (JSON)：**
```json
{
    "title": "两数之和",
    "difficulty": "Easy",
    "content": "给定一个整数数组 nums...",
    "template": "#include <iostream>\nusing namespace std;\n\nint main() {\n    return 0;\n}",
    "testCases": [
        {
            "input": "2 7 11 15\n9",
            "expected": "0 1"
        },
        {
            "input": "3 2 4 3\n6",
            "expected": "1 2"
        }
    ]
}
```

**响应结果 (Response)：**

成功响应 (201 Created)：
```json
{
    "id": 1,
    "title": "两数之和"
}
```

| 字段名 | 类型 | 说明 |
|--------|------|------|
| id | int | 题目 ID |
| title | string | 题目标题 |

失败响应：

| 状态码 | 字段名 | 说明 |
|--------|--------|------|
| 400 | error | Invalid JSON |
| 400 | error | Missing required fields: title, difficulty, content |
| 400 | error | Invalid difficulty. Must be Easy, Medium, or Hard |
| 500 | error | Failed to create problem |

---

### 4.2 删除题目

**接口名称及作用：** 管理员删除指定题目，级联删除关联的测试用例。

**请求路径 (Path)：** `/api/admin/problems/:id`

**请求方式 (Method)：** `DELETE`

**请求参数 (Parameters)：**

| 位置 | 字段名 | 类型 | 是否必填 | 说明 |
|------|--------|------|----------|------|
| Path | id | int | 是 | 题目 ID（正整数） |

**响应结果 (Response)：**

成功响应 (200 OK)：
```json
{
    "message": "Problem deleted successfully"
}
```

| 字段名 | 类型 | 说明 |
|--------|------|------|
| message | string | 成功消息 |

失败响应：

| 状态码 | 字段名 | 说明 |
|--------|--------|------|
| 400 | error | Invalid problem ID |
| 404 | error | Problem not found |
| 500 | error | Failed to delete problem |

---

## 静态资源

| 路径 | 说明 |
|------|------|
| `/` | 静态文件服务根目录，映射到 `./public` 目录 |
| `/index.html` | 大屏落地页 |
| `/login.html` | 登录页 |
| `/register.html` | 注册页 |
| `/problem_list.html` | 题目列表页 |
| `/problem.html?id=:id` | 题目详情页 |
| `/admin.html` | 管理后台页 |