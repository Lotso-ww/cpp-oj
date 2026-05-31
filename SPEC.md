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

---

## 2.1 项目目录结构

```
cpp-oj/
├── backend/                    # C++ 后端
│   ├── main.cpp               # 程序入口
│   ├── server.cpp/h           # HTTP 服务器
│   ├── api/                   # API 路由处理
│   │   ├── auth.cpp/h         # 认证相关
│   │   ├── problems.cpp/h     # 题目相关
│   │   ├── submissions.cpp/h  # 提交相关
│   │   └── admin.cpp/h        # 管理相关
│   ├── db/                    # 数据库操作
│   │   └── mysql.cpp/h
│   ├── judge/                 # 判题引擎
│   │   ├── compiler.cpp/h     # 编译
│   │   ├── sandbox.cpp/h      # 沙箱
│   │   └── spj.cpp/h          # 特判
│   ├── model/                 # 数据模型
│   └── utils/                 # 工具函数
├── frontend/                  # 前端资源
│   ├── index.html             # 主页/题目列表
│   ├── login.html             # 登录页
│   ├── register.html          # 注册页
│   ├── problem.html           # 题目详情页
│   ├── admin.html             # 后台管理页
│   ├── css/
│   │   └── style.css          # 样式
│   └── js/
│       ├── api.js             # API 调用封装
│       ├── auth.js            # 认证状态管理
│       └── app.js             # 页面逻辑
├── sql/
│   └── init.sql               # 数据库初始化脚本
├── SPEC.md                    # 本文档
└── README.md                  # 项目说明
```

---

## 2.2 API 边界

### 认证 API

| 方法 | 路径 | 说明 | 权限 |
|------|------|------|------|
| POST | /api/auth/register | 用户注册 | 公开 |
| POST | /api/auth/login | 用户登录 | 公开 |
| POST | /api/auth/logout | 用户登出 | 需认证 |
| GET | /api/auth/me | 获取当前用户信息 | 需认证 |

### 题目 API

| 方法 | 路径 | 说明 | 权限 |
|------|------|------|------|
| GET | /api/problems | 获取题目列表 | 公开 |
| GET | /api/problems/:id | 获取题目详情 | 公开 |
| POST | /api/problems | 创建题目 | 管理员 |
| PUT | /api/problems/:id | 更新题目 | 管理员 |
| DELETE | /api/problems/:id | 删除题目 | 管理员 |

### 提交 API

| 方法 | 路径 | 说明 | 权限 |
|------|------|------|------|
| POST | /api/submissions | 提交代码 | 需认证 |
| GET | /api/submissions | 获取提交历史 | 需认证 |
| GET | /api/submissions/:id | 获取提交详情 | 需认证 |

---

## 2.3 前后端关联

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

---

**关键组件**：
- Web 服务器：C++ + cpp-httplib（单进程部署）
- 数据库：MySQL（题目持久化, 提交记录/代码暂不持久化）
- 判题沙箱：进程级（cgroups/namespaces 限制 CPU/内存/时间）
- 语言支持：C/C++（g++ 编译运行）

---

## 3. 非功能需求

| 维度 | 要求 |
|------|------|
| 性能 | 支持 10-50 并发，判题超时限制 5s/题, <500ms响应 |
| 可扩展性 | 模块化设计，便于后续增加语言支持 |
| 安全 | 代码沙箱隔离，防止恶意代码执行 |
| 部署 | 单机器运行，后续可 Docker 化 |

---

## 4. 边缘场景与异常处理

- **编译超时**：代码 10s 内无法编译，判定为编译失败
- **运行超时**：单测试点 5s 无输出，强制终止
- **内存限制**：沙箱内单进程内存上限 256MB
- **恶意代码检测**：禁止 fork、exec、system 等危险调用
- **SPJ 校验失败**：记录详细错误日志，便于调试

---

## 5. 数据库 schema

```sql
users (id, username, password_hash, role ENUM('user','admin'), created_at)
problems (id, title, description, difficulty ENUM('easy','medium','hard'), 
          test_cases JSON, spj_code TEXT, time_limit, memory_limit, created_at)
submissions (id, user_id, problem_id, language, code, status, 
             exec_time, exec_memory, result_detail, created_at)
```

---

## 6. TODO 清单

### Phase 1 - 基础设施
- [ ] 项目结构搭建（C++ 后端 + 前端目录）
- [ ] MySQL 数据库初始化（schema 创建）
- [ ] cpp-httplib 引入与基础 server 骨架

### Phase 2 - 用户系统
- [ ] 用户注册/登录 API
- [ ] JWT 认证中间件
- [ ] 角色权限区分（user/admin）

### Phase 3 - 题目系统
- [ ] 题目 CRUD API（管理员）
- [ ] 题目列表 + 搜索/筛选 API
- [ ] 题目详情 API

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

### Phase 6 - 集成与测试
- [ ] API 联调
- [ ] 判题流程测试
- [ ] 异常场景测试

---

## 7. 验收标准

1. **用户系统**：可注册、登录，管理员可区分
2. **题目管理**：管理员可新增/编辑/删除题目，普通用户仅可查看
3. **代码提交**：用户可提交 C/C++ 代码，实时返回判题结果
4. **判题正确性**：SPJ 多测能正确判断答案对错，超时/编译失败有明确提示
5. **界面可用**：前端可正常浏览题目、编辑代码、查看历史
6. **安全隔离**：恶意代码（死循环、Fork）不会影响系统稳定性

| # | 标准 |
|---|------|
| 1 | 管理员可成功新增题⽬并在前端列表看到 |
| 2 | 普通⽤⼾可查看题⽬、在线编辑 C++ 代码并提交 |
| 3 | 代码在 <5s 超时限制内执⾏并返回结果 |
| 4 | 正确判断 AC/WA/TLE/RE 并反馈给⽤⼾ |
| 5 | 管理员可删除题⽬ |
| 6 | 普通⽤⼾⽆法访问管理接⼝ |
| 7 | 部署⽂档完整，单机可运⾏ |
| 8 | ⻚⾯⽆需刷新可完成⼀次完整提交 |
| 9 | 新⽤⼾可注册账号并登录 |