# cpp-oj

[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![Language](https://img.shields.io/badge/C%2B%2B-17-00599C.svg)](https://isocpp.org/)
[![Platform](https://img.shields.io/badge/platform-Linux-lightgrey.svg)]()
[![Build](https://img.shields.io/badge/build-CMake-064F8C.svg)](https://cmake.org/)
[![Database](https://img.shields.io/badge/MySQL-8.0-4479A1.svg)](https://www.mysql.com/)
[![Web Test](https://img.shields.io/badge/E2E-Playwright-2EAD33.svg)](https://playwright.dev/)

一个基于 C++17 构建的轻量级在线判题系统（Online Judge），面向教学与小规模训练场景，提供从题目录入、代码提交、沙箱编译执行到结果判定的完整闭环。

## 项目背景与目标

当前主流 OJ 系统多以 Java、Go、Python 等托管运行时语言实现，依赖容器或复杂的资源隔离层。对于 C++ 教学场景，平台自身亦应保持轻量、低延迟与可控。本项目目标在于：

- 以原生 C++ 实现 Web 服务、数据库访问与会话管理，剥离不必要的中间件开销；
- 在单机部署形态下提供端到端的判题体验，编译与运行可控、可观测；
- 对代码执行采用进程级隔离与资源约束（CPU、内存、墙钟时间），满足教学场景的安全基线；
- 通过清晰的分层架构（Handler / Service / Model / DB）为后续演进（如容器化隔离、提交持久化、预编译缓存）保留工程接口；
- 提供与生产代码同等级别的自动化测试保障，包括 GoogleTest 单元测试、curl 接口集成测试与 Playwright Web 端到端测试。

## 核心架构与特性

- **分层后端**：基于 `cpp-httplib` 单头文件 HTTP 框架，按 `server -> router -> handler -> service -> model -> db -> utils` 分层，单一可执行文件部署。
- **判题执行器**：`ExecutorService` 使用 `fork` 派生子进程隔离用户代码，通过 `g++` 编译、`pipe + execvp` 运行，配合 `setrlimit` 施加 CPU 与内存约束，并提供 AC / WA / CE / RE / TLE / MLE 六态判定；同时支持 `/api/run` 试运行与 `/api/submit` 提交评测两种入口。
- **MySQL 自定义连接池**：基于 `mysqlclient` C API 实现对象池，避免短连接开销，配合 `Config` 单例完成参数注入。
- **会话认证**：基于 `SessionManager` 维护内存态会话，配合 `HttpOnly` Cookie 完成普通用户与管理员的鉴权，附带 `/api/me` 自检端点。
- **角色化访问控制**：管理员接口与会话严格绑定，未登录或非管理员请求统一返回 401 / 403。
- **静态前端**：原生 HTML / CSS / JavaScript 单页交互，无任何前端构建工具链；落地页、列表页、详情页、后台页统一由后端静态目录挂载。
- **在线代码编辑器**：基于 Ace Editor 1.23.4（CDN 引入）提供 C/C++ 语法高亮、行号、当前行高亮与焦点边框态。
- **题目描述渲染**：通过 marked 12.0.0 将后台 Markdown 文本实时渲染为 HTML，统一前端展示。
- **统一配置中心**：`yaml-cpp` 加载 `config/config.yaml`，覆盖数据库、网络、连接池、超时与日志。
- **可观测性**：自研 `Logger` 模块，支持控制台与文件双通道，按 `info / warn / error` 分级。
- **多层测试矩阵**：GoogleTest 覆盖配置、日志、连接池、模型与各 Service / Handler；bash + curl 覆盖 HTTP 接口契约；Playwright 覆盖前端交互与会话流程。

## 技术栈构成

### 后端

- **语言与标准**：C++17（`GCC >= 9`，推荐 GCC 11 / 13）。
- **构建系统**：CMake（`>= 3.10`）。
- **HTTP 框架**：[cpp-httplib](https://github.com/yhirose/cpp-httplib)（单头文件，已纳入 `src/utils/httplib.h`）。
- **数据库**：MySQL 8.0（`mysqlclient` / `libmysql`）。
- **配置解析**：[yaml-cpp](https://github.com/jbeder/yaml-cpp)。
- **JSON 序列化**：[jsoncpp](https://github.com/open-source-parsers/jsoncpp)。
- **密码哈希**：[Botan 2](https://botan.randombit.net/)（bcrypt 算法）。
- **加密依赖**：OpenSSL（会话签名与可选 HTTPS 通道）。
- **判题沙箱**：Linux 进程级隔离（`fork + execvp + setrlimit`），墙钟超时默认 5 秒。
- **运行环境**：Linux（Ubuntu 22.04 / 24.04 LTS 已验证）。

### 前端

- **基础规范**：HTML5 / CSS3 / ES6+ JavaScript，无打包与构建步骤。
- **代码编辑器**：[Ace Editor 1.23.4](https://ace.c9.io/)（通过 `cdnjs` 引入 `ace.js` + `mode-c_cpp.js`）。
- **Markdown 渲染**：[marked 12.0.0](https://marked.js.org/)（通过 jsDelivr CDN 引入）。
- **Web 字体**：[霞鹜文楷 Web Font 1.7.0](https://github.com/lxgw/LxgwWenkaiTC)（`lxgw-wenkai-webfont`，通过 jsDelivr CDN 引入）。
- **CDN 源**：`cdn.jsdelivr.net`、`cdnjs.cloudflare.com`。

### 测试与质量保障

- **单元 / 集成测试**：[GoogleTest](https://github.com/google/googletest) + GoogleMock（`libgtest-dev`、`libgmock-dev`）。
- **接口契约测试**：bash + curl 脚本（参见 `tests/integration/cookie_hardening.sh` 与 `基于curl的接口自动化测试文档.md`）。
- **Web 端到端测试**：[Playwright](https://playwright.dev/)（通过 `npx --no-install playwright-cli` 调用，详见 `web自动化测试文档-playwright.md`）。
- **浏览器引擎**：Playwright 内置 Chromium / Firefox / WebKit，无需额外系统浏览器。

## 导航指引

- 部署与运行：[DEPLOY.md](DEPLOY.md)
- API 边界与字段定义：[API.md](API.md)
- 需求规格与验收标准：[SPEC.md](SPEC.md)
- 项目依赖清单：[dependence.md](dependence.md)
- Web 自动化测试（Playwright）：[web自动化测试文档-playwright.md](web自动化测试文档-playwright.md)
- 接口自动化测试（curl）：[基于curl的接口自动化测试文档.md](基于curl的接口自动化测试文档.md)