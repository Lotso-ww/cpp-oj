# OJ 系统规格说明书

## 1. 业务目标与功能范围

**核心定位**：企业内部/个人使用的代码在线评测系统，仿 LeetCode 风格。

**目标用户**：
- 普通用户：注册登录、浏览题目、提交代码、查看提交历史
- 管理员：题目完整 CRUD（增删改查）

**核心功能**：
- 用户系统（注册、登录、角色区分）
- 题目列表（搜索/筛选）
- 题目详情（含描述、测试用例）
- 代码在线编辑与提交
- 判题引擎（服务端编译执行 + 进程级沙箱 + SPJ 多测）
- 提交历史查询

---

## 2. 技术架构

```
┌─────────────────────────────────────────────────────┐
│                     前端                            │
│   HTML + CSS + JS (原生)                            │
│   - 用户界面（题目列表、详情、代码编辑器）            │
│   - 管理后台（题目 CRUD）                           │
│   前端页面：登录页、注册页、题目列表页、题目详情页、后台管理页
└─────────────────────┬───────────────────────────────┘
                        │ HTTP API
┌─────────────────────▼───────────────────────────────┐
│                     后端                            │
│   C++ (cpp-httplib)                                │
│   - Auth: JWT / Session                             │
│   - API: 题目、提交、用户、管理                     │
│   - Judge: 代码编译 + 沙箱执行 + SPJ 校验           │
└─────────────────────┬───────────────────────────────┘
                        │
┌─────────────────────▼───────────────────────────────┐
│                   MySQL                             │
│   - users (id, username, password, role)            │
│   - problems (id, title, description, difficulty,   │
│               test_cases, spj_code, ...)            │
│   - submissions (id, user_id, problem_id,          │
│                  language, code, result, ...)       │
└─────────────────────────────────────────────────────┘
```

### 2.1 技术选型

| 组件 | 技术 | 说明 |
|------|------|------|
| Web 服务器 | C++ + cpp-httplib | 单进程部署 |
| 数据库 | MySQL | 题目持久化，提交记录/代码暂不持久化 |
| 判题沙箱 | 进程级 cgroups/namespaces | 限制 CPU/内存/时间 |
| 语言支持 | C/C++ | g++ 编译运行 |

### 2.2 JWT 认证

| 项目 | 说明 |
|------|------|
| 算法 | HS256 |
| 过期时间 | 24 小时 |
| payload | `{user_id, username, role}` |
| 存储 | 前端 localStorage |
| 传递方式 | `Authorization: Bearer <token>` |

### 2.3 编译配置

| 项目 | 配置 |
|------|------|
| 编译器 | g++ |
| C++ 标准 | `-std=c++17` |
| C 标准 | `-std=c11` |
| 编译超时 | 10 秒 |
| 编译选项 | `-O2 -pipe` |

### 2.4 判题流程

```
提交代码 → 编译 → 编译成功?
                              ├─ 失败 → compile_error
                              └─ 成功 → 逐测试点执行
                                           │
                    ┌──────────────────────┼──────────────────────┐
                    ▼                      ▼                      ▼
               超时(>5s)             运行时错误             正常退出
                    │                      │                      │
                    ▼                      ▼                      ▼
           time_limit_exceeded      runtime_error          比对输出
                                                          ├─ 匹配 → next
                                                          └─ 不匹配 → wrong_answer
                                          
所有测试点通过 → accepted
```

### 2.5 SPJ 校验器

| 项目 | 说明 |
|------|------|
| 编写语言 | Python 3 |
| 执行方式 | 子进程运行，stdin/stdout 交互 |
| 输入 | 标准输入传给被测程序 |
| 对比 | SPJ 读取正确输出和用户输出，比较后返回 0/1 |

---

## 3. 项目目录结构

```
cpp-oj/
├── SPEC.md
├── README.md
├── CMakeLists.txt              # 后端构建配置
├── config/
│   └── config.yaml             # 配置文件
├── database/
│   └── init.sql                # 数据库初始化脚本
├── src/
│   ├── main.cc                 # 程序入口
│   ├── server/
│   │   ├── server.cc           # HTTP 服务器
│   │   ├── server.h
│   │   └── router.cc           # 路由处理
│   ├── handler/
│   │   ├── problem_handler.cc  # 题目相关 API
│   │   ├── problem_handler.h
│   │   ├── submit_handler.cc   # 代码提交执行
│   │   ├── submit_handler.h
│   │   ├── auth_handler.cc     # 登录注册
│   │   ├── auth_handler.h
│   │   ├── admin_handler.cc    # 管理接口
│   │   └── admin_handler.h
│   ├── service/
│   │   ├── problem_service.cc  # 题目业务逻辑
│   │   ├── problem_service.h
│   │   ├── executor_service.cc # 代码执行服务
│   │   ├── executor_service.h
│   │   ├── auth_service.cc     # 认证业务逻辑
│   │   └── auth_service.h
│   ├── model/
│   │   ├── problem.h            # 题目数据模型
│   │   ├── problem.cc
│   │   ├── test_case.h         # 测试用例模型
│   │   ├── test_case.cc
│   │   ├── user.h              # 用户模型
│   │   └── user.cc
│   ├── db/
│   │   ├── connection_pool.cc  # MySQL 连接池
│   │   └── connection_pool.h
│   └── utils/
│       ├── logger.cc           # 日志工具
│       ├── logger.h
│       ├── config.cc           # 配置加载
│       └── config.h
├── public/
│   ├── index.html              # 题目列表页
│   ├── login.html              # 登录页
│   ├── register.html           # 注册页
│   ├── problem.html            # 题目详情页
│   ├── admin.html              # 管理后台页
│   ├── css/
│   │   └── style.css           # 样式文件
│   └── js/
│       ├── api.js              # API 调用封装
│       ├── auth.js             # 认证状态管理
│       ├── problem.js          # 题目列表逻辑
│       ├── problem_detail.js   # 题目详情逻辑
│       ├── submit.js           # 提交执行逻辑
│       └── admin.js            # 管理后台逻辑
└── tests/
    ├── unit/
    │   ├── problem_test.cc
    │   └── executor_test.cc
    └── integration/
        └── api_test.cc
```

---

## 4. API 边界

### 通用错误响应

所有 API 错误返回格式：
```json
{
  "code": 错误码,
  "message": "错误描述"
}
```

常用错误码：
| code | 说明 |
|------|------|
| 400 | 请求参数错误 |
| 401 | 未认证或 token 失效 |
| 403 | 无权限 |
| 404 | 资源不存在 |
| 500 | 服务器内部错误 |

### 认证 API

| 方法 | 路径 | 说明 | 权限 |
|------|------|------|------|
| POST | /api/auth/register | 用户注册 | 公开 |
| POST | /api/auth/login | 用户登录 | 公开 |
| POST | /api/auth/logout | 用户登出 | 需认证 |
| GET | /api/auth/me | 获取当前用户信息 | 需认证 |

#### 请求/响应格式

**POST /api/auth/register**
```json
// Request
{"username": "testuser", "password": "123456"}

// Response 200
{"code": 0, "message": "success", "data": {"user_id": 1, "username": "testuser"}}
```

**POST /api/auth/login**
```json
// Request
{"username": "testuser", "password": "123456"}

// Response 200
{"code": 0, "message": "success", "data": {"token": "eyJhbGciOiJIUzI1NiJ9...", "user": {"id": 1, "username": "testuser", "role": "user"}}}
```

**GET /api/auth/me**
```json
// Response 200
{"code": 0, "message": "success", "data": {"id": 1, "username": "testuser", "role": "user"}}
```

### 题目 API

| 方法 | 路径 | 说明 | 权限 |
|------|------|------|------|
| GET | /api/problems | 获取题目列表 | 公开 |
| GET | /api/problems/:id | 获取题目详情 | 公开 |
| POST | /api/problems | 创建题目 | 管理员 |
| PUT | /api/problems/:id | 更新题目 | 管理员 |
| DELETE | /api/problems/:id | 删除题目 | 管理员 |

#### 请求/响应格式

**GET /api/problems**
```json
// Query: ?page=1&page_size=20&difficulty=easy&search=字符串
// Response 200
{
  "code": 0,
  "message": "success",
  "data": {
    "total": 100,
    "page": 1,
    "page_size": 20,
    "list": [
      {"id": 1, "title": "两数之和", "difficulty": "easy", "tags": ["数组","哈希"]}
    ]
  }
}
```

**GET /api/problems/:id**
```json
// Response 200
{
  "code": 0,
  "message": "success",
  "data": {
    "id": 1,
    "title": "两数之和",
    "description": "给定一个整数数组...",
    "difficulty": "easy",
    "tags": ["数组","哈希"],
    "hints": "可以使用哈希表来优化",
    "time_limit": 5000,
    "memory_limit": 256,
    "template_code": "#include <iostream>\nusing namespace std;",
    "sample_cases": [
      {"input": "1 2", "output": "3"},
      {"input": "4 5", "output": "9"}
    ]
  }
}
```

**POST /api/problems**
```json
// Request
{
  "title": "两数之和",
  "description": "给定一个整数数组...",
  "difficulty": "easy",
  "tags": "数组,哈希",
  "hints": "可以使用哈希表",
  "time_limit": 5000,
  "memory_limit": 256,
  "template_code": "#include <iostream>",
  "spj_code": null,
  "test_cases": [
    {"input": "1 2", "output": "3", "is_sample": true, "score": 10},
    {"input": "4 5", "output": "9", "is_sample": false, "score": 10}
  ]
}

// Response 200
{"code": 0, "message": "success", "data": {"id": 1}}
```

### 提交 API

| 方法 | 路径 | 说明 | 权限 |
|------|------|------|------|
| POST | /api/submissions | 提交代码 | 需认证 |
| GET | /api/submissions | 获取提交历史 | 需认证 |
| GET | /api/submissions/:id | 获取提交详情 | 需认证 |

#### 请求/响应格式

**POST /api/submissions**
```json
// Request
{"problem_id": 1, "language": "cpp", "code": "#include <iostream>..."}

// Response 200 (同步返回结果)
{
  "code": 0,
  "message": "success",
  "data": {
    "submission_id": 100,
    "status": "accepted",
    "exec_time": 45,
    "exec_memory": 2048,
    "result_detail": [
      {"case_id": 1, "status": "accepted", "exec_time": 10, "exec_memory": 1024},
      {"case_id": 2, "status": "accepted", "exec_time": 35, "exec_memory": 2048}
    ]
  }
}
```

**GET /api/submissions**
```json
// Query: ?page=1&page_size=20
// Response 200
{
  "code": 0,
  "message": "success",
  "data": {
    "total": 50,
    "page": 1,
    "page_size": 20,
    "list": [
      {"id": 100, "problem_id": 1, "problem_title": "两数之和", "status": "accepted", "exec_time": 45, "created_at": "2026-05-31 10:00:00"}
    ]
  }
}
```

**GET /api/submissions/:id**
```json
// Response 200
{
  "code": 0,
  "message": "success",
  "data": {
    "id": 100,
    "problem_id": 1,
    "problem_title": "两数之和",
    "language": "cpp",
    "code": "#include <iostream>...",
    "status": "accepted",
    "exec_time": 45,
    "exec_memory": 2048,
    "result_detail": [
      {"case_id": 1, "status": "accepted", "exec_time": 10, "exec_memory": 1024},
      {"case_id": 2, "status": "accepted", "exec_time": 35, "exec_memory": 2048}
    ],
    "created_at": "2026-05-31 10:00:00"
  }
}
```

#### 提交状态枚举

| status | 说明 |
|--------|------|
| pending | 等待判题 |
| judging | 判题中 |
| accepted | 通过 |
| wrong_answer | 答案错误 |
| time_limit_exceeded | 超时 |
| runtime_error | 运行时错误 |
| compile_error | 编译错误 |

---

## 5. 前后端关联

```
┌─────────────┐     HTTP JSON      ┌─────────────┐
│  前端页面   │ ◄───────────────► │   后端 API  │
└─────────────┘                    └─────────────┘
      │                                  │
      ▼                                  ▼
┌─────────────┐                    ┌─────────────┐
│  登录页    │ ── /api/auth/* ──► │  认证模块   │
│  register  │                    │             │
├─────────────┤                    ├─────────────┤
│  题目列表页 │ ── /api/problems ─► │  题目模块   │
│  index     │                    │             │
├─────────────┤                    ├─────────────┤
│  题目详情页 │ ── /api/problems ─► │             │
│  problem   │      +submissions   │             │
├─────────────┤                    ├─────────────┤
│  后台管理页 │ ── /api/problems ─► │  管理模块   │
│  admin     │      (CRUD)        │             │
└─────────────┘                    └─────────────┘
```

### 页面与 API 对应关系

| 前端页面 | 调用 API | 功能 |
|----------|----------|------|
| login.html | POST /api/auth/login | 登录 |
| register.html | POST /api/auth/register | 注册 |
| index.html | GET /api/problems | 展示题目列表 |
| problem.html | GET /api/problems/:id, POST /api/submissions | 题目详情+提交代码 |
| admin.html | GET/POST/PUT/DELETE /api/problems | 题目增删改查 |

### 认证流程

1. 用户登录 → 后端返回 JWT token
2. 前端存储 token (localStorage)
3. 后续请求带上 `Authorization: Bearer <token>` 头
4. 后端验证 token 并解析用户角色

### 前端部署

前端静态文件部署在 `public/` 目录，服务器配置：
- `GET /` → `public/index.html`
- `GET /*.html` → `public/*.html`
- `GET /css/*` → `public/css/*`
- `GET /js/*` → `public/js/*`
- API 请求 → `/api/*`

---

## 6. 非功能需求

| 维度 | 要求 |
|------|------|
| 性能 | 支持 10-50 并发，判题超时限制 5s/题, <500ms响应 |
| 可扩展性 | 模块化设计，便于后续增加语言支持 |
| 安全 | 代码沙箱隔离，防止恶意代码执行 |
| 部署 | 单机器运行，后续可 Docker 化 |
| 分页 | 列表 API 默认 page_size=20 |

---

## 7. 边缘场景与异常处理

- **编译超时**：代码 10s 内无法编译，判定为编译失败
- **运行超时**：单测试点 5s 无输出，强制终止
- **内存限制**：沙箱内单进程内存上限 256MB
- **恶意代码检测**：禁止 fork、exec、system 等危险调用
- **SPJ 校验失败**：记录详细错误日志，便于调试
- **测试点串行执行**：逐个运行测试点，遇错即停，不继续执行后续测试点

---

## 8. 数据库 schema

```sql
-- 用户表
users (
    id          BIGINT PRIMARY KEY AUTO_INCREMENT,
    username    VARCHAR(64) UNIQUE NOT NULL,
    password_hash VARCHAR(128) NOT NULL,
    role        ENUM('user', 'admin') DEFAULT 'user',
    created_at  DATETIME DEFAULT CURRENT_TIMESTAMP
)

-- 题目表
problems (
    id          BIGINT PRIMARY KEY AUTO_INCREMENT,
    title       VARCHAR(256) NOT NULL,
    description TEXT NOT NULL,
    difficulty  ENUM('easy', 'medium', 'hard') NOT NULL,
    spj_code    TEXT,                          -- 特判代码（可选）
    template_code TEXT,                        -- 代码模板（可选）
    tags VARCHAR(512), -- 标签，逗号分隔
    hints TEXT,                          -- 提示（可选）
    time_limit  INT DEFAULT 5000,             -- 时间限制(ms)
    memory_limit INT DEFAULT 256,              -- 内存限制(MB)
    created_at  DATETIME DEFAULT CURRENT_TIMESTAMP
)

-- 测试用例表 (1:N 关联 - 一个题目对应多个测试用例)
test_cases (
    id          BIGINT PRIMARY KEY AUTO_INCREMENT,
    problem_id  BIGINT NOT NULL,
    input       TEXT NOT NULL,                -- 输入数据
    output      TEXT NOT NULL,                -- 期望输出
    is_sample   BOOLEAN DEFAULT FALSE,        -- 是否为示例用例
    score       INT DEFAULT 10,              -- 该用例分值
    FOREIGN KEY (problem_id) REFERENCES problems(id) ON DELETE CASCADE
)

-- 提交记录表
submissions (
    id          BIGINT PRIMARY KEY AUTO_INCREMENT,
    user_id     BIGINT NOT NULL,
    problem_id  BIGINT NOT NULL,
    language    ENUM('cpp', 'c') NOT NULL,
    code        TEXT NOT NULL,
    status      ENUM('pending', 'judging', 'accepted', 'wrong_answer', 
                     'time_limit_exceeded', 'runtime_error', 'compile_error') DEFAULT 'pending',
    exec_time   INT,                          -- 执行时间(ms)
    exec_memory INT,                          -- 内存占用(KB)
    result_detail TEXT,                       -- 判题详情(JSON数组)
    created_at  DATETIME DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (user_id) REFERENCES users(id),
    FOREIGN KEY (problem_id) REFERENCES problems(id)
)
```

#### result_detail JSON 格式

```json
[
  {"case_id": 1, "status": "accepted", "exec_time": 10, "exec_memory": 1024, "output": "3"},
  {"case_id": 2, "status": "wrong_answer", "exec_time": 5, "exec_memory": 512, "expected": "9", "actual": "8"}
]
```

---

## 9. TODO 清单

### Phase 1 - 基础设施
- [x] 项目结构搭建（C++ 后端 + 前端目录）
- [ ] MySQL 数据库初始化（schema 创建）
- [ ] cpp-httplib 引入与基础 server 骨架
- [ ] 配置管理（日志、数据库连接）

### Phase 2 - 用户系统
- [ ] 用户注册/登录 API
- [ ] JWT 认证中间件
- [ ] 角色权限区分（user/admin）

### Phase 3 - 题目系统
- [ ] 题目 CRUD API（管理员）
- [ ] 题目列表 + 搜索/筛选 API
- [ ] 题目详情 API
- [ ] 题目数据模型映射

### Phase 4 - 判题引擎
- [ ] C/C++ 代码编译（g++）
- [ ] 进程级沙箱（cgroups + setrlimit）
- [ ] SPJ 多测校验器
- [ ] 判题结果存储

### Phase 5 - 前端
- [ ] 页面结构（题目列表、详情、代码编辑器）
- [ ] 类 LeetCode 深色主题样式
- [ ] 用户注册界面
- [ ] 用户登录/登出界面
- [ ] 提交代码 + 查看结果
- [ ] 提交历史页面
- [ ] 管理后台（题目增删改查）

### Phase 6 - 安全与部署
- [ ] 管理员权限校验
- [ ] 用户认证
- [ ] 基础输入校验
- [ ] 部署文档

### Phase 7 - 集成与测试
- [ ] API 联调
- [ ] 判题流程测试
- [ ] 异常场景测试


---

## 10. 验收标准

1. **用户系统**：可注册、登录，管理员可区分
2. **题目管理**：管理员可新增/编辑/删除题目，普通用户仅可查看
3. **代码提交**：用户可提交 C/C++ 代码，实时返回判题结果
4. **判题正确性**：SPJ 多测能正确判断答案对错，超时/编译失败有明确提示
5. **界面可用**：前端可正常浏览题目、编辑代码、查看历史
6. **安全隔离**：恶意代码（死循环、Fork）不会影响系统稳定性

| # | 标准 |
|---|------|
| 1 | 管理员可成功新增题目并在前端列表看到 |
| 2 | 普通用户可查看题目、在线编辑 C++ 代码并提交 |
| 3 | 代码在 <5s 超时限制内执行并返回结果 |
| 4 | 正确判断 AC/WA/TLE/RE 并反馈给用户 |
| 5 | 管理员可删除题目 |
| 6 | 普通用户无法访问管理接口 |
| 7 | 部署文档完整，单机可运行 |
| 8 | 页面无需刷新可完成一次完整提交 |
| 9 | 新用户可注册账号并登录 |