# C++ OJ 项目复盘文档

> 本文档为 cpp-oj 项目的面试复盘整理，覆盖项目全局架构与判题核心模块，逐文件、逐行精讲，附带面试官追问预案与已知工程 gap。

---

## 总览

项目是一个基于 C++17 的轻量级在线判题系统（Online Judge），面向教学与小规模训练场景，提供从题目录入、代码提交、沙箱编译执行到结果判定的完整闭环。

### 核心技术点地图（面试高频考点）

| # | 模块 | 关键技术点 |
|---|------|-----------|
| 1 | 整体架构 | `server→router→handler→service→model→db→utils` 六层分层，单可执行文件部署，Pimpl 编译防火墙 |
| 2 | 判题沙箱（核心） | `fork + execlp(g++)` 编译、`fork + execl` 运行、`pipe+dup2` 重定向、`select` 多路复用超时、`setrlimit`(RLIMIT_AS/RLIMIT_CPU)、`wait4+rusage`、SIGKILL vs SIGXCPU、AC/WA/CE/RE/TLE/MLE 六态判定 |
| 3 | 连接池 | Meyers 单例、`mutex+condition_variable+queue` 生产者消费者、`mysql_ping` 自动重连、utf8mb4 |
| 4 | 会话认证 | Cookie/Session（非 JWT）、32 位随机 token、bcrypt(cost=10)、HttpOnly+SameSite=Lax+Max-Age、后台线程清理(CAS 幂等启停) |
| 5 | 日志/配置 | 策略模式 LogStrategy、`LogMessage` 析构即提交 RAII、yaml-cpp + Pimpl、5 个单例 |
| 6 | 前端 | 零构建原生 JS、`api.js` 统一 `{ok,status,data}`、`credentials:same-origin`、Ace 编辑器(字体加载时序坑)、marked 渲染、sessionStorage+`/api/me` 双源登录态、防开放重定向 `safeReturn` |
| 7 | 测试 | GTest 13 套（含 cookie 加固 A4/B2/B3 回归、连接池并发 200 ops、executor 全 verdict 分支）、curl/Python API 契约、Playwright E2E |

### 可主动指出的工程 Gap（体现判断力）

1. `createProblem` 插入 problem + test_cases 未包事务
2. `admin_handler` 仍用 `cookie.find` 子串解析（与 auth_handler 已修复的 bug 同型）
3. Model 返回裸指针，应改 `unique_ptr`
4. 沙箱仅 setrlimit，无 cgroup/chroot/seccomp
5. `run_handler` 逐用例重复编译
6. `User::update` 不哈希密码（与 save 不一致）
7. `main.cc:43` `ENABLE_CONSOLE_LOG_STRATEGY()` 覆盖 `InitLogger` 的策略选择
8. `FileLogStrategy` 每条日志重新 open/close 文件
9. `Router` 类内部死代码（`routes_` map 和 `get/post/...` 方法未使用）
10. `executor_service` 中 `if(timeout)/else` 两个分支代码完全相同（冗余）
11. `memoryKb` 字段从未赋值，MLE 实际从未被判定

---

## 复盘推进路径

- **Round 1** 项目全局与启动链路：架构图 + main / config / logger / server / router
- **Round 2** 判题核心 `executor_service.cc` 逐行讲解（最高频考点，最深入）
- **Round 3** 认证全链路：bcrypt → User → AuthService → SessionManager → auth_handler Cookie 三态
- **Round 4** 数据层：连接池 + 三个 Model + 事务/sql注入/utf8mb4
- **Round 5** 前端架构与 api.js / problem_detail.js / Ace 时序 / 登录态双源
- **Round 6** 测试体系 + 模拟面试问答

---

# Round 1：项目全局与启动链路

## 1.1 全局架构图（必须背下来）

```
                       浏览器 (原生 HTML/CSS/JS)
                            │ HTTP + Cookie
                            ▼
┌─────────────────────────────────────────────────────────┐
│ main.cc  ──信号(SIGINT/SIGTERM)──► Server::stop()  优雅停机  │
│              │                                            │
│              ▼                                            │
│  oj::Server  (Pimpl 隐藏 httplib::Server)                  │
│      │ start(): Config→ConnectionPool单例触发→Router.setup  │
│      ▼                                                     │
│  httplib::Server  ──set_mount_point("/", "./public")──── 静态前端
│      │                                                 └── set_error_handler
│      ▼                                                     │
│  Router::setupRoutes()  ── 注册 10 个 API endpoint ──► 各 Handler::staticMethod
│      │
│      ▼
┌──────────────────────────────────────────────────────┐
│ Handler 层 (解析JSON/校验/鉴权/组装响应)              │
│  ProblemHandler / SubmitHandler / RunHandler         │
│  AuthHandler    / AdminHandler                       │
└──────┬───────────────────────────────────────────────┘
       ▼
┌──────────────────────────────────────────────────────┐
│ Service 层 (业务编排)                                 │
│  ProblemService   ExecutorService(★判题核心★)         │
│  AuthService      SessionManager                     │
└──────┬───────────────────────────────────────────────┘
       ▼
┌──────────────────────────────────────────────────────┐
│ Model 层 (Active Record，直接操作 MySQL)             │
│  Problem  TestCase  User                             │
└──────┬───────────────────────────────────────────────┘
       ▼
┌──────────────────────────────────────────────────────┐
│ DB 层   ConnectionPool 单例 (mutex+cv+queue+mysql_ping)│
└──────────────────────────────────────────────────────┘

横切：Config(单例/yaml) │ Logger(单例/策略模式) │ PasswordUtil(bcrypt)
```

**面试官一句概括**：「这是一个基于 cpp-httplib 的六层单可执行文件 OJ：`server→router→handler→service→model→db`，横切 Config/Logger/Password 三大单例。判题核心走 fork+exec+setrlimit 沙箱，认证走 Cookie+服务端 Session。没有用 Spring/ORM 这类重型框架，强调轻量与原生控制力。」

## 1.2 一次"用户提交代码"请求的完整调用链（背熟这条）

```
浏览器 problem_detail.js submitCode()
  ──POST /api/submit {code, problemId}──► (credentials:same-origin 带 cookie)
http://Server listen
  └─► httplib 内部匹配路由
        └─► Router 注册的 lambda
              └─► SubmitHandler::submitCode(req, res)              submit_handler.cc
                    ├─ Json::parseFromStream  → 失败 400
                    ├─ 校验 code/problemId 非空    → 失败 400
                    ├─ Problem::findById(id)                     problem.cc  (手 release)
                    │     └─► ConnectionPool::getConnection()     connection_pool.cc
                    │           └─► mysql_ping 活性检测 → 失败重建
                    │     └─► mysql_query(SELECT...) + fetch_row
                    │     └─► TestCase::findByProblemId() (1+N 防 N+1)
                    ├─ test_cases==空 → 400
                    ├─ ExecutorService::getInstance().compileAndRun(code, cases)  ★
                    │     ├─ fork + execlp("g++",...) → pipe 捕获 stderr → select 超时
                    │     └─ fork + execl(用户程序) → setrlimit(AS/CPU) → select → wait4+rusage
                    │           ├─ SIGKILL(主动) / SIGXCPU(RLIMIT_CPU) → TLE
                    │           ├─ segfault → RE
                    │           ├─ exit!=0 → RE
                    │           └─ normalizeOutput 比对 → 不等则 WA
                    └─ 组装 JSON {status:AC/CE/RE/TLE/MLE/WA, stdout, time,...} 200
```

记住：**handler 做参数校验，service 做编排，model 做 DB，executor 做隔离执行**。这条链路是后面所有轮次的主线。

---

## 1.3 启动链路：`src/main.cc`（66 行）

先看入口，这是面试"先讲项目怎么跑起来的"必答题。

### 启动顺序（8 个动作）

```
① 注册 SIGINT/SIGTERM → signalHandler (main.cc:28)
② 配置路径可命令行注入 (main.cc:31-34)  —— 便于多环境/测试
③ Config::getInstance().load() (main.cc:36-40) —— Meyers 单例首次访问触发构造
④ InitLogger() (main.cc:42) tactical: 根据 logFile 选 file/console 策略
⑤ ENABLE_CONSOLE_LOG_STRATEGY() (main.cc:43) ⚠ 强制覆盖为 console
⑥ Server server; g_server = &server (main.cc:49-50)
⑦ SessionManager::startCleanupThread(60) —— 每 60s 扫过期 session
⑧ server.start() (main.cc:57) —— 内部触发连接池首次构造 + 注册路由 + listen 阻塞
退出时：stopCleanupThread() join 线程 → 析构 server → 析构单例(连接池等)
```

### 关键设计点

**① 优雅停机**（main.cc:16-23）

为什么要在信号里调 `stop()`？因为 `svr.listen()` 是**阻塞**调用，只有 `stop()` 才能让它返回，从而走完 main 的正常返回路径，触发所有对象的析构链（连接池 close、session join、临时文件清理）。否则直接 `_exit` 会跳过析构。

> **面试官可能追问**：你在信号处理函数里调 `LOG` 和 `stop()`，这违反 async-signal-safe 原则吗？
> **预案答**：严格说，信号处理函数中只应调 async-signal-safe 函数（如 `write`、`kill`）。`LOG` 用了 `std::ofstream` 和 `mutex`，`Server::stop()` 内部设 flag 也非严格安全。工程上选择"可接受风险"——因为信号到达时主线程在 `listen` 阻塞，争用 mutex 概率低。**更严谨做法**：信号 handler 只设 `volatile sig_atomic_t flag`，主线程一个单独线程 `wait` 该 flag 调用 `stop()`。

**⑤ 配置覆盖的不一致点**（main.cc:43）⚠ **可主动指出**

`InitLogger()` 已经会根据 config 里 `logFile` 是否为空自动选择 file/console 策略；但下一行又强行 `ENABLE_CONSOLE_LOG_STRATEGY()`。这意味着**即使你配置了 logFile，最终也只会打到控制台**。

> 面试话术：「这里有一个我后来发现的设计瑕疵：`ENABLE_CONSOLE_LOG_STRATEGY()` 会无条件覆盖 `InitLogger` 的判断，导致配置文件的 logging.file 永远不生效。生产部署时应该删掉这一行，遵循配置优先。」

**⑧ 连接池的惰性初始化时机**（main.cc:57 → server.cc）

`server.start()` 内部有 `(void)ConnectionPool::getInstance();`——故意触发单例首次构造。这意味着 **Config 必须先 load 完，再进 start**，否则连接池会用空配置连接 MySQL。这个顺序约束是隐式的，配错了就崩。

---

## 1.4 配置中心：`src/utils/config.h` + `config.cc`（Pimpl + yaml-cpp）

### ① Meyers Singleton（config.cc:30-33）

```cpp
Config& Config::getInstance() {
    static Config instance;   // C++11 保证线程安全初始化
    return instance;
}
```

- **C++11 标准 §6.7**：函数内 `static` 局部变量的初始化是线程安全的（编译器会插一个 `__cxa_guard_acquire` 的锁）。
- 这是项目里 5 个单例（Config/Logger/ConnectionPool/ExecutorService/SessionManager）的统一写法。
- **相对饿汉/双检锁的优势**：无需手写锁，无初始化顺序问题，析构由运行时负责。

> **追问**：Meyers 单例的析构顺序问题？
> **答**：析构在 `atexit` 中按构造逆序进行。配置先于连接池构造→连接池先析构→没问题。但若把单例构件跨翻译单元强耦合（如 A 单例析构时调 B 单例），就有"destruction order fiasco"。本项目单例之间只在 `getInstance()` 时耦合，不在析构时反查，安全。

### ② Pimpl Idiom（config.h:39-40）

```cpp
struct Impl;                            // 前置声明
std::unique_ptr<Impl> pImpl;            // 只指针，不透出 Impl 成员
```

- **意义**：`config.h` 不需要 `#include <yaml-cpp/yaml.h>`，也不需要暴露所有字段，**头文件依赖降为 0**。
- 编译提速：yaml-cpp 是个大头，藏到 .cc 里，所有 include config.h 的文件不再因 yaml-cpp 改动而全量重编。
- **代价**：每次 getter 都有一层 `pImpl->` 间接寻址（极小，可忽略）。

### ③ YAML 解析的容错（config.cc:63-89）

- 每段 `if (config["database"])` 判存在再取值，**缺段不报错**（默认 0/空）。
- 整个 load 包在 `try/catch (YAML::Exception&)`，返回 bool 表示成败。

> **追问**：缺字段默认 0 会不会埋雷？
> **预案答**：会。比如 `connectionPoolSize=0` 时，连接池 `initPool` 循环 0 次，池空，所有 DB 调用永远等待 → 死锁。生产应该加 **schema 校验**（必填字段缺失/越界直接报错退出）。这是当前缺的一项。

### ④ reset() 的设计（config.cc:35-52）

构造函数先调 reset 保证字段都清零，避免未加载时 getter 返回野值。也支持热重载（虽然 main 只调一次）。

---

## 1.5 日志模块：`src/utils/logger.h/cc`（策略模式 + RAII 析构提交）

### ① 策略模式：LogStrategy + Console/File 实现（logger.h:28-56）

```
LogStrategy  (抽象基类: virtual SyncLog)
   ├── ConsoleLogStrategy  → std::cout
   └── FileLogStrategy     → ofstream append
```

- **多态**：`Logger::_strategy` 是 `unique_ptr<LogStrategy>`，运行时 `UseConsoleLogStrategy()` / `UseFileLogStrategy(path)` 可热切换输出目的地。
- **开闭原则**：要加远程日志/Syslog/ELK，只需新增子类，不改 Logger。
- **每个 Strategy 自带 `std::mutex`**：保证多线程并发 `LOG` 不串行交错。

### ② RAII"析构即提交"惯用法（logger.h:66-89, logger.cc:109-115）✨ 核心

```cpp
LOG(INFO) << "x=" << x;   // 等价于：
// logger.getInstance()(INFO, __FILE__, __LINE__)  返回一个临时 LogMessage 对象
//     << "x=" << x;     operator<< 收集进 _loginfo
// ;                     分号 → 临时对象析构 → ~LogMessage() 调 _strategy->SyncLog
```

- 关键在于：**`LogMessage` 是临时对象**，整条语句结束时（分号处）析构，自动把组装好的全条日志一次性 flush。
- 好处 1：写法像 `std::cout`，无需手写 `<< std::endl`。
- 好处 2：把"组装日志"和"输出日志"解耦——组装在临时对象生命周期内，输出在析构那一刻，**跨线程不会撕裂**（因为整条是 std::string 一次性传给 SyncLog，SyncLog 内加锁）。

> **追问**：`LOG(INFO) << f();` 里 `f()` 抛异常会怎样？
> **答**：临时 `LogMessage` 析构**不会**被调用（因为构造未完成），整条日志不会输出。如果想保证异常时也打印，应改成显式 `try` 或用 RAII 守护加 catch。本项目未做，因为日志主要用于运行信息，不依赖异常路径。

### ③ 时间戳线程安全（logger.cc:30）

```cpp
localtime_r(&currentTime, &dataTime);   // 线程安全版，写入调用者提供的 tm
```

- 相对 `localtime()` 返回 `static tm*` 不安全。`localtime_r` 的 `_r` = reentrant（可重入）。
- 这是面试常被问的"线程安全问题"考点，主动提一句加分。

### ④ 一个性能瑕疵可主动指出（logger.cc:66-72）⚠

```cpp
void FileLogStrategy::SyncLog(...) {
    std::ofstream out(_logfilepath, std::ios::app);   // 每条日志 open
    out << message << "\n";
    out.close();                                      // 每条日志 close
}
```

每条日志都会**重新 open/close 文件**，每条至少一次 `open` syscall。高 QPS 时是瓶颈。

> **预案答**：「FileLogStrategy 应该把 `std::ofstream` 提升为成员、构造时就 open、析构时关闭，SyncLog 只 `<<` + 周期性 flush。再上一层可以做异步日志队列（`mpsc` 环形缓冲）+ 后台线程批量 flush，避免日志路径阻塞业务线程。」

### ⑤ InitLogger 与 main 的覆盖问题呼应

回顾 `main.cc:42-43`：

```cpp
InitLogger();                          // 可能选了 file
ENABLE_CONSOLE_LOG_STRATEGY();         // 又强制改成 console ← bug
```

这是 Round 1.3 提到的点，此处形成"理论 vs 实现"的闭环——能讲出来很显功。

---

## 1.6 HTTP 服务器：`src/server/server.h/cc`（Pimpl 编译防火墙）

### ① 为什么必须 Pimpl（核心思想）

`httplib.h` 是单头文件框架但**有 2 万多行**，里面塞了 `socket`/`openssl`/`thread` 各种头。

- **若把 `httplib::Server svr;` 直接放 server.h 成员**：每个 `#include "server.h"` 的文件（main、router、所有 handler、所有测试）都会拖入这 2 万行，全项目编译时间会爆炸。
- **用 Pimpl**：只放 `std::unique_ptr<Impl> pImpl`，前置声明 `struct Impl`，真正定义藏到 .cc（只有 .cc 一处 include `httplib.h`），**编译依赖隔离在一处**。

> **追问**：编译防火墙的量化收益？
> **答**：改动一次 httplib.h，原本全项目 50+ 翻译单元要重编；Pimpl 后只有 server.cc 重编。增量编译从分钟级降到秒级。

### ② `start()` 里的隐含时序契约（server.cc:31-37）

```cpp
LOG(...) << "Initializing connection pool...";
(void)ConnectionPool::getInstance();   // 触发单例构造 → 真去连 DB
LOG(...) << "Connection pool initialized";
Router::setupServer(pImpl->svr);        // 注册所有路由
return pImpl->svr.listen(host, port);   // 阻塞，stop() 才返回
```

- **`(void)`**：故意丢弃返回值，**只是为了其副作用（首次访问触发 Meyers 单例构造）**。这是 C++ 惰性初始化的常见写法。
- 必须先 `config.load()` 再 `start()`，否则 `getServerHost()` 返回空串，listen 失败。

### ③ 析构兜底（server.cc:21-23）

```cpp
Server::~Server() { stop(); }   // 即使忘了显式 stop，析构也停
```

`stop()` 内部有 `if (running)` 守卫，**幂等**——多次调用安全。

### ④ `listen()` 阻塞 vs 非阻塞

`svr.listen(host, port)` 会**阻塞**直到收到 `stop()` 才返回。`stop()` 内部 `svr.stop()` 设置标志位让 accept 退出。这就是为什么 main 信号 handler 直接调 `stop()`。

---

## 1.7 路由注册：`src/server/router.h/cc`

### ① 10 个 API 端点一览（必须背）

```
公开:  GET  /api/problems       POST /api/submit     POST /api/login
       GET  /api/problems/:id   POST /api/run        POST /api/logout
                                   POST /api/register    GET  /api/me
管理:  POST /api/admin/problems   DELETE /api/admin/problems/:id
```

- `/api/submit`（评测，只返总状态） vs `/api/run`（运行测试，返每用例明细+可加customCases）。**这两个端点的设计差异是面试亮点**。

### ② 路径参数 `:id` 由 cpp-httplib 原生支持

handler 内用 `req.path_params.find("id")` 拿值。getProblem/deleteProblem 都这样取。

### ③ 静态资源挂载（router.cc:52）

```cpp
svr.set_mount_point("/", "./public");
```

- 把 `/` 整个挂到 `./public` 目录，与 `/api/*` 路由共存。
- 请求 `/login.html` → 直接读文件返；请求 `/api/problems` → 走 API handler。
- 前端是**纯原生 HTML/JS**，零后端模板渲染，全部靠 fetch 调 API 动态填充。

### ④ 自定义 404（router.cc:54-60）⚠ 一个隐蔽点

```cpp
if (res.status == -1) { res.status = 404; ... }
```

- cpp-httplib 用 **`-1` 作为"未设置状态码"哨兵值**，不是 0 也不是 200。
- 路由未匹配 → error handler 触发 → 改成标准 JSON 404。

### ⑤ 一个值得指出的"死代码"瑕疵（router.h:7-27）⚠

```cpp
class Router {
    using Handler = std::function<void(const std::string&, int)>;
    void get/post/put/del(...);
    bool route(...);
private:
    std::unordered_map<std::string, Route> routes_;
};
```

这个 `Router` 类的 `routes_` map 和 `get/post/route` 方法**完全没被使用**——实际路由全靠 cpp-httplib 内置路由表。这是历史重构遗留。

> **预案话术**：「这是早期想自研路由层但后来直接用 cpp-httplib 内置路由的遗留。可清理掉，免得误导。」

### ⑥ lambda 桥接 = 解耦

```cpp
svr.Post("/api/login", [](const httplib::Request& req, httplib::Response& res) {
    AuthHandler::login(req, res);
});
```

httplib 接收 lambda 签名固定是 `(req, res)`，业务方 Handler 是静态方法。lambda 这层桥让**框架与业务解耦**——以后换框架，只需改 lambda 内调用。

### ⑦ 缺中间件 = 重复鉴权（架构意不足）

- admin_handler 和 auth_handler 各自重复实现"`oj_session` Cookie 解析 + validateSession"。
- 没有统一 `AuthMiddleware(req) → SessionInfo`。

> **预案话术**：「可重构出 `requireLogin(req)` 和 `requireAdmin(req)` 两个中间件，handler 内一行调用即可，把鉴权从业务中剥离。当前是重复实现，且 auth_handler 用精确解析、admin_handler 还用脆弱子串解析，两边不一致——这是后面的 Round 3 会展开讲的安全细节。」

---

## 1.8 Round 1 面试官追问预案速查（贴墙背）

| 问题 | 答案骨架 |
|------|---------|
| 用三句话介绍你的 OJ 项目 | 六层单可执行 C++ OJ；fork+exec+setrlimit 沙箱判题；Cookie+Service-side Session 鉴权；原生前端 + GTest/curl/Playwright 三层测试 |
| 项目的初始化顺序为什么这样排 | Config 必须先于连接池（连接池读 Config）；Logger 复用 Config；server.start 内惰性触发连接池和路由注册；最后 startCleanupThread |
| 为什么要用 Pimpl | 隔离 httplib.h / yaml-cpp 大头，编译防火墙；ABI 稳定；增量编译从分钟降到秒级 |
| 为什么要用单例 | Config/Logger/Pool/Executor/SessionManager 都是"全局唯一、生命周期=进程、需统一访问点"的资源；Meyers 单例线程安全且无需手写锁 |
| 信号 handler 里直接调 stop 安全吗 | 严格说违反 async-signal-safe，工程上接受；更稳是用 `sig_atomic_t flag` + 单独线程轮询 |
| 听说你的日志有 bug？ | 主动暴露：ENABLE_CONSOLE_LOG 覆盖 InitLogger；FileLogStrategy 每条 open/close 性能差；建议提升 ofstream + 异步队列 |
| 路由是怎么匹配的 | cpp-httplib 内置路由表，支持 `:id` 路径参数；静态资源 `set_mount_point("/", "./public")`；未命中走 error_handler 改 -1 为 404 |
| 启动失败会怎样 | Config::load 失败退出 1；server.start 失败先 stopCleanupThread 再退出 1；任意异常未捕获则 abort 留 core |

---

## 1.9 Round 1 作业

1. **自己用语言复述一遍**：从浏览器发请求到 server 接收，路径上经过哪些类、哪些单例？
2. **脑海过一遍** main.cc 的 8 个启动动作顺序，以及"如果换序会出什么问题"。
3. **找出 3 处工程瑕疵**（提示：main.cc:43、FileLogStrategy open/close、Router 死代码），并想"如果让你重构会怎么改"。

---

**Round 1 总结**：项目的骨架 = main 编排启动 + Config/Logger 两大横切单例 + Server(Pimpl) + Router 注册路由 + Handler/Service/Model/DB 四层业务。骨架清楚了，后面每轮只会是在这条主线上"扎进具体模块"。

---

# Round 2：判题核心 `executor_service.cc` 逐行剖析

## 2.1 顶层视角：判题的核心矛盾

OJ 的判题本质是个**"在服务器上跑用户写的任意代码"**的问题。这有三重风险：
1. **资源耗尽**：用户代码死循环、申请 100G 内存、fork 炸弹
2. **信息泄露**：用户代码读 `/etc/passwd`、读数据库配置
3. **逃逸破坏**：用户代码删文件、起网络连接打内网

业界两条路：
- **重**：Docker / cgroup v2 / namespace / seccomp / chroot（LeetCode、Codeforces 用）
- **轻**：`fork + exec + setrlimit` 进程级沙箱（本项目，教学场景可接受）

本项目选了轻量路线，所以这一轮要会讲：**"我清楚轻量方案的边界——能挡住资源类攻击，但挡不住文件系统/网络/fork 炸弹。我也能讲重方案怎么做。"**

记住这句开场：「判题沙箱是 `fork + exec + setrlimit + select 超时杀进程 + wait4 取资源使用` 的经典 Unix 模型。它能正确判定 AC/WA/CE/RE/TLE/MLE，但安全层面只挡资源类，不挡 IO/网络——这是教学场景的权衡。」

## 2.2 数据结构（executor_service.h:8-26）

```cpp
enum class RunResult { SUCCESS, RUNTIME_ERROR, TIME_LIMIT_EXCEEDED, MEMORY_LIMIT_EXCEEDED, SYSTEM_ERROR };
struct ExecutionResponse {
    bool compileSuccess;
    std::string compileOutput;   // g++ 的 stderr（编译错误信息）
    int exitCode;
    std::string stdout, stderr;  // 用户程序的两个流
    long executionTimeMs;         // 来自 wait4 的 rusage.ru_utime
    long memoryKb;                // ⚠ 实际未赋值——这是个 gap
    RunResult result;
    std::string errorMessage;    // 关键："Wrong Answer" 也走这里
};
```

### 关键设计点 ①：状态机是两层

- **RunResult 枚举**：5 种"运行结果"（不含 WA！）
- **WA 是用 `errorMessage = "Wrong Answer"` 复用 SUCCESS 表示的**（executor_service.cc:391-392）

> **追问**：为什么 WA 不直接做成 enum 值？
> **预案答**：「这是当时的接口权衡——`RunResult` 是"运行层"结果（代码是否正常跑完），WA 属于"答案层"结果（输出是否正确）。当时选择复用 errorMessage 区分。**更干净的做法**应该是单独加 `WRONG_ANSWER` 枚举值，让 handler 直接 switch 不依赖字符串比较，确切避免拼写错误和魔法串。这是后来我反思后能指出的改进点。」

### 关键设计点 ②：`memoryKb` 字段从未赋值 ⚠

全文件搜不到给 `memoryKb` 赋值的语句。它是结构体里的死字段。

- `RLIMIT_AS` 设置了 64MB 虚拟内存上限，理论上 MLE 该靠 `ENOMEM` 或 SIGSEGV 检测，但**当前代码没区分内存信号**——所有非 SIGKILL/SIGXCPU 信号都被归到 RUNTIME_ERROR。
- 所以 **MLE 状态实际从未被产生**。API.md 写了 MLE 但实现上没真正判定出来。

> **预案答**：「这里有个我已知的 gap：MLE 没真正实现。正常做法是 `setrlimit(RLIMIT_AS)` 后用户 `malloc` 失败返回 `ENOMEM`，用户代码若不检查会继续跑导致其他错误，最终被误判成 RE。要正确判 MLE，需要在子进程里读 `/proc/<pid>/status` 的 VmRSS，或用 `wait4` 的 `rusage.ru_maxrss`（Linux 是 kB）跟阈值对比。」

## 2.3 临时文件名随机化（executor_service.cc:44-62）

```cpp
std::random_device rd;
std::mt19937 gen(rd());
std::uniform_int_distribution<> dis(100000, 999999);
std::string filename = tmpDir + "/source_" + std::to_string(dis(gen)) + ".cpp";
```

- **`random_device`**：OS 级 CSPRNG（Linux 是 `/dev/urandom`），不可预测。
- **`mt19937`**：Mersenne Twister 伪随机数生成器，速度快、周期长（2^19937-1），播种一次后重复调用不慢。
- 目的：**避免路径预测竞态**。如果文件名可预测，攻击者可在判题间隙用恶意文件替换 source_1.cpp。

> **追问**：为什么不用 `mkstemp` / `tmpfile`？
> **答**：`mkstemp` 适合一次性临时文件，但这里源文件路径要传给 `g++ -o exec_<rand>`，可执行文件名也要随机化，统一用 mt19937 控制更直观。**更严谨**应该用 `mkstemp` + `mkostemp` 避免 TOCTOU，但当前路径已够教学场景。

## 2.4 编译流程 `compileCode`（48-173 行）—— 逐行精讲

这是"父进程 fork 子进程跑 g++，靠 pipe 收 stderr，靠 select 超时杀进程"的范式。runExecutable 是它的三管道版。先彻底吃透 compileCode。

### 第一步：建 errPipe 捕获 g++ 错误输出（80-85）

```cpp
int errPipe[2];
if (pipe(errPipe) < 0) {            // pipe() 创建一对管道：errPipe[0]读端 / errPipe[1]写端
    compileOutput = "Pipe creation failed: " + std::string(strerror(errno));
    unlink(sourcePath.c_str());
    return false;
}
```

**`pipe()` 知识点**（面试必问）：
- `pipe(fd)` 创建一对内核缓冲的管道，`fd[0]` 只读、`fd[1]` 只写。
- 数据流：`write(fd[1])` → 内核缓冲 → `read(fd[0])`。
- 容量：Linux 默认 64KB（`fs.pipe-max-size` 可调），写满则 write 阻塞。
- 当所有写端都关闭，读端会读到 EOF（`read` 返回 0）——**这是"对方写完了"的信号**。

**为什么不直接捕获 g++ 的 stderr？** 因为 g++ 是在子进程跑的，子进程的 stderr 默认连到父进程的终端，但 fork 之后子进程是独立进程，**直接 `<<` 到终端会有竞态乱序**。通过 pipe 把子进程 stderr 重定向到父进程可控的 fd，是 Unix 经典做法。

### 第二步：fork 子进程（87-104）

```cpp
pid_t pid = fork();
if (pid < 0) {                      // fork 失败（罕见，通常资源耗尽）
    ...
}

if (pid == 0) {                    // 子进程分支（pid==0）
    close(errPipe[0]);              // 子进程不需要读端，关闭
    dup2(errPipe[1], STDERR_FILENO);// 把管道写端复制到 fd=2 (stderr)
    close(errPipe[1]);              // 关掉原来的写端 fd（已 dup 到 2）

    execlp("g++", "g++", "-o", execPath.c_str(), sourcePath.c_str(), 
           "-std=c++17", "-O2", "-Wall", nullptr);
    _exit(1);                       // execlp 只有失败才会返回，立即 _exit 避免刷 stdio 缓冲
}

close(errPipe[1]);                  // 父进程关闭写端（关键！否则读端永远读不到 EOF）
```

**`fork()` 知识点**：
- 调用一次，返回两次。父进程返回子进程 pid（>0），子进程返回 0，失败返回 -1。
- 子进程是父进程的**几乎完整拷贝**（COW：copy-on-write，实际不复制内存页直到写）。
- 文件描述符表也拷贝一份，所以父子都有 errPipe 的两端——**必须各自关闭不需要的一端**，否则会泄漏 fd 且读不到 EOF。

**`dup2(oldfd, newfd)` 知识点**：
- 把 `oldfd` 复制到 `newfd`，如果 `newfd` 已开则先关闭。结果：`newfd` 也指向 `oldfd` 同一文件。
- 这里 `dup2(errPipe[1], STDERR_FILENO)` 让 fd=2（即 stderr）也指向管道写端。
- 之后子进程任何写到 stderr 的内容（g++ 错误信息）实际都进了管道。

**`execlp` 知识点**（关键）：
- `execlp(file, arg0, arg1, ..., nullptr)`：用 PATH 搜索找到可执行文件，**替换当前进程的代码段、数据段、堆栈**，从新程序的 main 开始执行。
- **成功不返回**，只有失败才返回 -1。所以下一行 `_exit(1)` 只在 execlp 失败时执行。
- 用 `execlp` 而非 `execv`：`p` 表示自动查 PATH，方便写 `g++` 不用全路径。
- `arg0` 习惯传程序名本身（argv[0]）。
- **变参列表必须以 `nullptr` 哨兵结尾**，否则未定义行为——这是经典坑。

**为什么用 `_exit` 而不是 `exit`？**
- `exit()` 会：刷新 stdio 缓冲 → 调 atexit 注册函数 → 调析构。这些会在 fork 后的子进程里**重新执行一遍**，可能污染父进程共享的状态（如 std::ofstream 未刷盘的缓冲会被刷两次）。
- `_exit()` 直接走 syscall 退出，不刷缓冲、不调析构。
- 这是 fork-exec 模范里**必须知道的细节**：**子进程 exec 失败要 `_exit`，不要 `exit`**。

> **追问**：编译参数为什么是 `-std=c++17 -O2 -Wall`？为什么不开 `-Werror`？
> **答**：`-O2` 开优化让用户代码接近真实运行速度（学业 OJ 标配）；`-Wall` 出警告但保持编译通过；不开 `-Werror` 是怕用户代码写 `int x;` 未用告警被判 CE，太严影响判题体验。还可讨论加 `-lm`、`-DONLINE_JUDGE` 等。

### 第三步：父进程 select 等读 + 超时杀进程（106-142）

```cpp
close(errPipe[1]);                  // 父进程关写端（这一步必须在 fork 之后！）

fd_set readSet;
struct timeval tv;
tv.tv_sec = timeoutMs / 1000;
tv.tv_usec = (timeoutMs % 1000) * 1000;

std::string compileErrors;
bool timeout = false;

while (true) {
    FD_ZERO(&readSet);              // 每轮清空集合
    FD_SET(errPipe[0], &readSet);   // 把读端加入集合

    int selectResult = select(errPipe[0] + 1, &readSet, nullptr, nullptr, &tv);
    
    if (selectResult > 0) {         // 有数据可读或对方关闭
        char buf[4096];
        ssize_t n = read(errPipe[0], buf, sizeof(buf) - 1);
        if (n > 0) {
            buf[n] = '\0';
            compileErrors += buf;
        } else if (n == 0) {         // EOF：g++ 关闭了写端 → 编译结束
            break;
        }
    } else if (selectResult == 0) { // 超时
        timeout = true;
        kill(pid, SIGKILL);          // 立即杀子进程
        break;
    } else {
        break;                       // EINTR 等错误直接退出
    }

    int status;
    int ret = wait4(pid, &status, WNOHANG, nullptr);  // 非阻塞回收
    if (ret > 0) break;             // 子进程真退了
}
```

**`select()` 知识点**（面试必问）：
- `select(maxfd+1, &readfds, nullptr, nullptr, &tv)`：阻塞直到集合中任一 fd 可读，或 tv 超时。
- **`maxfd+1`**：select 用位图实现，要告诉它检查 0..maxfd 这些 fd。所以传最大 fd 值 + 1。
- **`fd_set` 位图**：FD_ZERO 清零、FD_SET 设位、FD_ISSET 测位。默认 FD_SETSIZE=1024，关联 fd > 1024 不能用 select（要改 poll/epoll）——本项目 fd 都很小不会超。
- **`timeval`**：`{tv_sec, tv_usec}` 微秒级超时。**注意 select 返回后 tv 会被改为剩余时间**（Linux），所以每轮都要重设 tv——这里每轮重设是对的。
- **返回值**：>0 就绪 fd 数；=0 超时；<0 出错（EINTR 可能是被信号中断）。

**`read()` 返回值含义**：
- `>0`：实际读到的字节数；
- `=0`：EOF，对方关闭写端（这是循环终止条件！）；
- `<0`：错误，可能 EAGAIN（非阻塞 fd）或 EINTR。

**`wait4(pid, &status, WNOHANG, &usage)` 知识点**：
- 等子进程退出，`WNOHANG` 表示**非阻塞**：子进程还没退就立即返回 0。
- 第 4 参数 `struct rusage*` 可以拿到子进程的资源使用（CPU 时间、最大 RSS）——`compileCode` 这里传 nullptr 因为不需要，但 `runExecutable` 会用 usage 取执行时间。
- **为什么要非阻塞穿插在 select 循环里？** 因为子进程可能先写入再退出，但写入到内核 pipe buffer 是异步的。单纯等待子进程退出会漏掉早期数据；单纯等管道 EOF 也会读到 0 拿到所有数据。**用 WNOHANG 探活让循环在子进程真的退出后立刻退出**，不必等下一次 select 超时。

> **追问 1**：为什么 select 返回后又调 wait4 而不是 break 立即 break？
> **答**：因为子进程可能写了一部分数据进 pipe 就退出，pipe 还没到 EOF。如果不读完 pipe 直接 wait4 阻塞，会漏数据。先 `read` 累积、读完 `n==0` 拿到 EOF 再退出才完整。是否还要再调一次 wait4 探活？这里在 select 间隙穿插一次 wait4 是为了让循环响应"子进程已退出但 pipe 还有数据未读完"的时序，确保循环能正常结束。
>
> **追问 2**：超时杀进程之后 wait4 还会执行吗？
> **答**：会。break 之后到 144 行 `close(errPipe[0])`，然后 149 行 `if (timeout) wait4(pid, &status, 0, &usage)` 阻塞回收——**杀进程后必须 wait 回收**，否则子进程变僵尸（Z 状态），长期运行下 PID 表满。

### 第四步：阻塞回收兜底（149-153）

```cpp
if (timeout) {
    wait4(pid, &status, 0, &usage);
} else {
    wait4(pid, &status, 0, &usage);
}
```

⚠ **冗余**：两个分支完全一样。这是个无意义的 if。简单的 `wait4(pid, &status, 0, &usage);` 就够。**可以主动指出**：「这里 if/else 两个分支代码完全相同，是冗余，可简化。」显示你看懂了细节。

**为什么必须再调一次 wait4？** 因为前面 select 循环里的 `wait4(WNOHANG)` 探活时，可能 ret=0（子进程没退）；循环 break 时也没保证 wait4 已经回收了进程。**最后必须用 `wait4(0)` 阻塞回收一次确保不残留僵尸**。`0`（无 WNOHANG）= 阻塞等。

### 第五步：WIFEXITED/WIFSIGNALED 判定结果（157-172）

```cpp
unlink(sourcePath.c_str());         // 删源文件

if (WIFEXITED(status)) {              // 正常退出（exit 或 main return）
    int exitCode = WEXITSTATUS(status);
    if (exitCode == 0) return true;     // 编译成功
    else {
        compileOutput = compileErrors; // 编译失败，错误信息从 pipe 收集
        return false;
    }
} else if (WIFSIGNALED(status)) {     // 被信号杀死
    compileOutput = "Compilation killed by signal " + std::to_string(WTERMSIG(status));
    unlink(execPath.c_str());           // 信号杀了不会有可执行文件产出，删掉残留
    return false;
}
compileOutput = compileErrors;
return false;
```

**`status` 宏知识点**（必背）：
- `WIFEXITED(status)`：正常退出（main return / `exit(n)`）？
- `WEXITSTATUS(status)`：取 exit code（0-255）
- `WIFSIGNALED(status)`：被信号杀死？
- `WTERMSIG(status)`：是哪个信号
- `WIFSTOPED(status)`：被 stop（暂停）？本项目没用
- 这是 POSIX 的标准 wait status 编码：低 7 位是信号号，第 8 位是 core dump，高 8 位是 exit code。

**核心判断逻辑**：
- exit 0 = 编译成功
- exit 非 0 = 编译失败（错误信息在 compileErrors）
- 信号杀死 = 时间到被 SIGKILL → 编译失败（消息告知是哪个信号）

注意：g++编译失败时 exit code 是 1，错误信息在 stderr，已被 pipe 收集。**这部分非常优雅**。

### 关键设计点 ③：源文件清理 vs 可执行文件清理

- 编译**成功**后，源文件 `unlink(sourcePath)`（155 行），可执行文件**保留**给 runExecutable 用。
- 编译**失败/被信号杀**，源文件和残留可执行文件都清掉。
- **临时文件全链路清理**，无残留。

## 2.5 运行流程 `runExecutable`（175-320 行）—— 三管道版

差异点：多两个管道（stdin 喂入、stdout 收输出），且 `setrlimit` 在子进程设资源限制。这部分和 compileCode 框架相同，只讲差异点。

### 三管道（181-185）

```cpp
int inputPipe[2], outputPipe[2], errorPipe[2];
pipe(inputPipe)  // 父写 → 子读 stdin
pipe(outputPipe) // 子写 stdout → 父读
pipe(errorPipe)  // 子写 stderr → 父读
```

数据流：
```
父进程:       写 inputPipe[1]                       读 outputPipe[0] / errorPipe[0]
               │                                      ▲                    ▲
               ▼                                      │                    │
              ┌──────────── pipe kernel buffer ─────────────┐
              ↓                                            │                    │
子进程:       stdin  ← dup2(inputPipe[0],  STDIN_FILENO)   │
              stdout → dup2(outputPipe[1], STDOUT_FILENO)──┘
              stderr → dup2(errorPipe[1],  STDERR_FILENO)──────────
```

### 子进程的 fd 关闭（196-207）

```cpp
if (pid == 0) {
    close(inputPipe[1]);    // 子不要 stdin 的写端
    close(outputPipe[0]);   // 子不要 stdout 的读端
    close(errorPipe[0]);    // 子不要 stderr 的读端

    dup2(inputPipe[0],  STDIN_FILENO);
    dup2(outputPipe[1], STDOUT_FILENO);
    dup2(errorPipe[1],  STDERR_FILENO);

    close(inputPipe[0]);    // dup 完关闭原端，避免 fd 泄漏
    close(outputPipe[1]);
    close(errorPipe[1]);

    setrlimit(...);         // 见 2.5 资源限制
    execl(executablePath.c_str(), executablePath.c_str(), nullptr);
    _exit(1);
}
```

**为什么这么多 close？** **fd 数量有限**（默认每进程 1024），且不关闭会泄漏到 exec 后的新程序里。**dup2 之后立即关原端**，因为 dup2 复制了一份，原端多余。**这是粗心最容易出 bug 的地方**——少关一端会导致父进程 read 永远读不到 EOF（因为子进程也持有写端）。

### 父进程喂输入（225-228）

```cpp
close(inputPipe[0]);                    // 父不要 stdin 的读端
if (!input.empty()) {
    write(inputPipe[1], input.c_str(), input.size());  // 把测试输入写进 stdin
}
close(inputPipe[1]);                     // 关闭写端，子进程读到 EOF 知道输入结束
```

**关键点**：写完必须 `close(inputPipe[1])`。否则子进程的 `cin >> x` 在读完输入后会**继续等待**，永远不返回 EOF，导致子进程 hang 死被 select 超时杀掉误判 TLE。

> **追问**：如果输入很大（超过 64KB pipe 容量）会怎样？
> **答**：`write` 会阻塞直到子进程读走部分腾出空间。但如果子进程还没开始读就先输出大量数据塞满 outputPipe，可能死锁——子进程想写 stdout 阻塞、父进程想写 stdin 也阻塞。
> **更稳做法**：把"写 stdin"和"读 stdout"放到不同线程，或用非阻塞 fd + epoll。本项目假设输入小（教学场景），未做。**这是面试可深挖的点**。

### 双 fd 多路复用 select（230-283）

```cpp
while (true) {
    FD_ZERO(&readSet);
    int maxFd = 0;
    if (outputPipe[0] >= 0) { FD_SET(outputPipe[0], &readSet); maxFd = std::max(maxFd, outputPipe[0]); }
    if (errorPipe[0] >= 0) { FD_SET(errorPipe[0], &readSet); maxFd = std::max(maxFd, errorPipe[0]); }
    int selectResult = select(maxFd + 1, &readSet, nullptr, nullptr, &tv);
    
    if (selectResult > 0) {
        if (FD_ISSET(outputPipe[0], &readSet)) {
            ssize_t n = read(outputPipe[0], buf, sizeof(buf) - 1);
            if (n > 0) stdoutContent += buf;
            // ⚠ 这里没有处理 n==0 关闭 outputPipe[0]！
        }
        if (FD_ISSET(errorPipe[0], &readSet)) {
            ssize_t n = read(errorPipe[0], buf, sizeof(buf) - 1);
            if (n > 0) stderrContent += buf;
        }
    } else if (selectResult == 0) {
        timeout = true; kill(pid, SIGKILL); break;
    } else break;

    int ret = wait4(pid, &status, WNOHANG, &usage);
    if (ret > 0) break;
}
```

### 资源限制 `setrlimit`（209-215）—— 沙箱的关键

```cpp
struct rlimit rl;
rl.rlim_cur = 64 * 1024 * 1024;     // 软 64 MB
rl.rlim_max = 128 * 1024 * 1024;    // 硬 128 MB
setrlimit(RLIMIT_AS, &rl);          // 限制虚拟地址空间

rl.rlim_cur = 60;                   // 软 60 CPU 秒
rl.rlim_max = 60;                   // 硬 60 CPU 秒
setrlimit(RLIMIT_CPU, &rl);         // 限制 CPU 时间
```

**`setrlimit` 知识点**（面试必问）：
- `rlimit { rlim_cur, rlim_max }` 软硬两限。软可由进程自己再调高到硬，硬只能降不能升。
- **必须在 `fork` 之后、`exec` 之前的子进程里设**——影响的是 exec 之后的新程序。父进程的 rlimit 不变。
- **`RLIMIT_AS`**：虚拟地址空间上限（malloc/new 实际向 OS 申请 mmap）。超过则 mmap 失败，C `new` 抛 `std::bad_alloc`，C `malloc` 返回 NULL。如果用户不检查则继续访问导致 SIGSEGV。
- **`RLIMIT_CPU`**：CPU 时间（秒）上限。**超过会发 SIGXCPU 信号** → 默认终止。这就是下面 TLE 的 "被动触发"。
- **为什么 cpu 设 60s 而 select 超时才 5s？** RLIMIT_CPU 是 CPU 时间（不包括 sleep/等 IO 的时间），select 超时是墙钟时间。10s 死循环占的是 CPU 时间，10s sleep 占的是墙钟时间但不占 CPU 时间——所以**两层超时互为补位**：
    - 死循环 / 复杂计算 → 占 CPU → 5 秒墙钟用满 select 杀掉 → TLE
    - sleep / read 阻塞 → 不占 CPU → 5 秒墙钟 select 杀掉 → TLE
    - 60s CPU 兜底防 select 没生效的极端情况

**软硬限差异**（面试可延伸）：
- 软限超触发 SIGXCPU（可被捕获，进程可能忽略继续跑）。
- 硬限超触发 SIGKILL（不可捕获，直接死）。
- 设软 60 相对宽松；生产 OJ 一般硬设更严（如硬 3s）。

### 安全性局限（必讲）⚠

| 攻击 | 当前沙箱挡不挡 | 真实生产 OJ 怎么挡 |
|------|--------|------|
| 死循环 | ✅ select 杀 | ✅ |
| 大内存 | ✅ RLIMIT_AS | ✅ + cgroup memory |
| fork 炸弹 | ❌ 未设 RLIMIT_NPROC | ✅ cgroup pids |
| 读 `/etc/passwd` | ❌ 未限文件 | ✅ chroot + seccomp |
| 网络请求 | ❌ | ✅ network namespace |
| 修改 `/tmp/oj_exec` 别人代码 | ⚠ 部分防（随机名） | ✅ 多用户隔离 |
| 起后门进程长驻 | ⚠ SIGKILL 后会清，但运行中可执行 | ✅ 进程命名空间 |

> **预案答**：「这是个**进程级沙箱，仅挡资源类攻击**。要堵上文件/网络/fork 漏洞，应升级到容器级隔离：用 Linux namespaces（mount/pid/net/user）做命名空间隔离、cgroup v2 做资源限制（内存 + CPU + pids）、seccomp 白名单系统调用限制、chroot 限制文件系统根。最方便的实现是 Docker + `--network=none --memory --cpus --pids-limit`。教学场景下我接受当前权衡，因为我可以预先控制用户群。」

> **追问**：为什么不用 seccomp 至少禁掉 `open` 系统调用？
> **答**：可以用，但实现复杂（要列白名单系统调用），而且 g++ 编译产物启动时会调 open 读 ld.so.cache、动态库，禁严会启动不起来。要么静态链接用户代码 + seccomp strict，要么用 seccomp-bpf 黑名单 `open(at)` + 限定路径。这是能讲但实现成本高的点。

### 执行时间统计（299）

```cpp
response.executionTimeMs = usage.ru_utime.tv_sec * 1000 + usage.ru_utime.tv_usec / 1000;
```

**`rusage` 知识点**：
- `ru_utime`：用户态 CPU 时间（`struct timeval`：秒+微秒）。
- `ru_stime`：内核态 CPU 时间。
- `ru_maxrss`：最大驻留集大小（Linux 单位是 KB，macOS 是字节，跨平台坑）。
- 本项目只取 `ru_utime` → 毫秒。**注意是 CPU 时间不是墙钟**——sleep 不算时间。这跟前端展示的 `executionTimeMs` 语义略不同，但够用。

### 结果判定（301-317）

```cpp
if (WIFEXITED(status)) {
    response.exitCode = WEXITSTATUS(status);
    if (response.exitCode != 0) response.result = RUNTIME_ERROR;   // 非0退出 = RE
    else response.result = SUCCESS;                                // 0退出 = AC候选
} else if (WIFSIGNALED(status)) {
    int sig = WTERMSIG(status);
    if (sig == SIGKILL || sig == SIGXCPU) {
        response.result = TIME_LIMIT_EXCEEDED;                     // SIGKILL(select杀)/SIGXCPU(CPU限) = TLE
    } else {
        response.result = RUNTIME_ERROR;                            // SIGSEGV/SIGFPE/SIGABRT = RE
    }
}
```

**信号映射**：
- SIGKILL（9）：父进程主动发 → TLE
- SIGXCPU（24, Linux）：RLIMIT_CPU 触发 → TLE
- SIGSEGV（11）：段错误 → RE
- SIGFPE（8）：除零 → RE
- SIGABRT（6）：abort/assert 失败 → RE

**为什么 SIGKILL 和 SIGXCPU 都判 TLE？**
- SIGKILL 是 select 超时后父进程主动发的（line 275 `kill(pid, SIGKILL)`）——明确表示墙钟超时。
- SIGXCPU 是 RLIMIT_CPU 触发的内核信号——表示 CPU 时间超。
- 两者都意味着"超时"，统一判 TLE 正确。

**MLE 缺失的根源**：内存超限时不发特别信号，`new` 失败抛 `bad_alloc` 若用户未 catch → SIGABRT → 被判 RE。或者 `malloc` 返回 NULL 用户访问 SIGSEGV → 被判 RE。**MLE 实际从不会被产生**——这是 2.2 节提到的 gap，与 `memoryKb` 从未赋值呼应。

## 2.6 编排层 `compileAndRun`（322-413）

```cpp
ExecutionResponse compileAndRun(code, testCases, compileTimeoutMs=10000, runTimeoutMs=5000) {
    finalResponse.result = SYSTEM_ERROR;                    // 默认系统错误
    
    if (sourceCode.empty()) return errorMessage="Empty source code";
    
    int compileTimeout = Config.getCompileTimeout();
    if (compileTimeoutMs > 0) compileTimeout = compileTimeoutMs;  // 参数优先于配置
    
    bool compileSuccess = compileCode(code, execPath, compileOutput, compileTimeout);
    if (!compileSuccess) return CE响应;
    
    finalResponse.compileSuccess = true;
    if (testCases.empty()) return SUCCESS响应;              // 编译成功但无用例
    
    for (testCase : testCases) {
        int runTimeout = Config.getRunTimeout();
        if (runTimeoutMs > 0) runTimeout = runTimeoutMs;
        
        ExecutionResponse runResp = runExecutable(execPath, input, runTimeout);
        
        if (TLE)  { unlink; return TLE; }
        if (RE)   { unlink; return RE; }
        
        expectedOutput = testCase.expected;
        if (!expectedOutput.empty() && normalizeOutput(stdout) != normalizeOutput(expected)) {
            result = SUCCESS;
            errorMessage = "Wrong Answer";          // ⚠ WA 复用 SUCCESS
            unlink;
            return;
        }
        
        // 追踪最后用例的输出（run_handler 在 all-AC 时也要显示 actual）
        finalResponse.stdout = runResp.stdout;
        ...
    }
    
    finalResponse.result = SUCCESS;
    unlink(execPath);
    return finalResponse;
}
```

### 关键设计点 ④：参数优先于配置

```cpp
int compileTimeout = Config::getInstance().getCompileTimeout();  // 默认从配置
if (compileTimeoutMs > 0) compileTimeout = compileTimeoutMs;     // 调用方传 >0 则覆盖
```

- 配置中心决定默认值，单次调用可覆盖。
- 测试代码 `compileAndRun(src, cases, 10000, 1000)` 显式传 1000ms 强行触 TLE，就是这个机制。
- **是可以重载 default 参数的好设计**。

### 关键设计点 ⑤：normalizeOutput（21-32）

```cpp
std::string normalizeOutput(std::string s) {
    std::string out;
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '\r' && i + 1 < s.size() && s[i+1] == '\n') continue;  // CRLF → LF
        out.push_back(s[i]);
    }
    while (!out.empty() && (out.back() == '\r' || out.back() == '\n')) {
        out.pop_back();    // 去尾部所有换行
    }
    return out;
}
```

为什么必须做：用户 C++ 输出 `"3.14\n"`，DB 存的 expected 是 `"3.14"`（管理员录入时可能没敲换行），直接 string == 会判 WA。**这是 OJ 的"输出归一化"硬需求**。

> **追问**：这种比较方式有什么坑？
> **答**：① 只去尾部换行不去前导——若用户输出 `\n3.14` 仍判 WA，合理；② 忽略 CRLF/LF 差异，但**不忽略中部多余空格**——若用户输出 `3.14 ` 末尾带空格判 WA，这才是严格判题。③ 行尾空格不去——可能误判（部分 OJ 会做"不去尾空格"的 spotless，有的会敏感）。**可改进**：增加可选的"忽略尾空格"模式。④ 不处理浮点容差——`3.14` vs `3.140` 判 WA，要 approximate 比较需特殊处理（专业 OJ 一般要求 fixed 输出位数）。

### 关键设计点 ⑥：所有 early return 都 `unlink(execPath)` ⚠

搜文件能看到 5 处 `unlink(execPath.c_str())`：TLE、RE、WA、正常 return、SUCCESS 路径。**保证可执行文件无残留**。

> **追问**：如果 `runExecutable` 中间抛异常怎么办？
> **答**：当前没 try/catch，异常会跳过所有 unlink，可执行文件残留——**这是 RAII 缺失**。**改进**：应该把 execPath 包成 RAII 守护对象，析构时 unlink。当前用裸字符串靠手动管理，正确但脆弱。

### 关键设计点 ⑦：每次调用反复 fork+exec g++

`/api/run` 逐用例重复编译：run_handler.cc 内对每个用例都调用一次 `compileAndRun(code, ...)`，每个用例都走完整 fork g++ + fork user program。3-5 用例 OK，但 50 用例就慢 50 次。

> **预案答**：「这是 executor 接口粒度的局限——`compileAndRun` 一次只能跑一个用例，调用方要对多用例自己循环。**重构方向**：拆出 `compile()` 和 `run(execPath, input)` 两个函数，run_handler 先 `compile` 一次再循环 `run`，编译只跑一次。注释里也写了这是已知改进点。」

## 2.7 整体数据流（背下来）

```
源代码 → createTempSourceFile(随机名)
       → fork + execlp("g++", -std=c++17 -O2)
              │ stderr → pipe → 父进程收集 compileErrors
              │ exit 0 → 编译成功，execPath 可用
              │ exit !=0 → 编译失败 → 返回 CE
              │ 信号杀 → 编译失败
       
       对每个 testCase:
       → fork + execl(execPath)
              │ 父写 stdin (input)
              │ 子进程 setrlimit(AS=64MB, CPU=60s)
              │ 子 stdout → pipe → 父收集
              │ 子 stderr → pipe → 父收集
              │ select(maxFd+1, [outputPipe, errorPipe], tv=5s)
              │   ├─ ready → read 累积
              │   ├─ timeout → kill(SIGKILL) → TLE
              │   └─ wait4(WNOHANG) 探退
              → wait4(0, &usage) 兜底回收
              → 解析 status:
                ├─ WIFEXITED 0 → SUCCESS 候选
                ├─ WIFEXITED !=0 → RE
                ├─ WIFSIGNALED SIGKILL/SIGXCPU → TLE
                └─ WIFSIGNALED 其他 → RE
              → normalizeOutput(stdout) != normalizeOutput(expected) → WA
       
       全通过 → AC（stdout 追踪最后一个用例实际输出）
       unlink(execPath) 清理
```

## 2.8 Round 2 面试官追问速查表（**贴墙背**）

| 问题 | 答案骨架 |
|------|---------|
| 介绍一下你的判题沙箱怎么实现的 | fork+exec+pipe+dup2+select+setrlimit+wait4+rusage 的经典 Unix 进程级沙箱 |
| 为什么选这个而不是 Docker | 教学场景，权衡控制力与部署复杂度，可挡资源类，不挡 IO/网络，能升级到 cgroup+namespace |
| 编译失败怎么知道？ | g++ 的 exit code 非 0，错误信息从 pipe 收集 → compileOutput 返前端 |
| 死循环怎么处理？ | select 墙钟 5s 超时 → kill SIGKILL → WIFSIGNALED + SIGKILL → TLE |
| RLIMIT_CPU 和 select 超时为啥都用 | CPU 不算 sleep，墙钟含 sleep，互补 |
| RLIMIT_AS 怎么判 MLE？ | 实际我代码没真正判 MLE——超过 AS 不会发特别信号，会被判 RE。这是已知 gap，应该读 ru_maxrss 或 /proc |
| fork 后没 pipe 写端关掉会怎样 | EOF 检测失败，父进程 read 永远阻塞，超时被杀误判 TLE |
| write stdin 不 close 父端? | 子进程永远不会收到 EOF，cin 一直阻塞，select 超时杀掉误判 TLE |
| WA 为什么用 errorMessage 不用 enum | 当时的接口权衡：RunResult 表示"运行层"结果，WA 是"答案层"。改进：加 WRONG_ANSWER enum 更干净 |
| 僵尸进程怎么避免 | 每次 fork 后必须 wait4 回收；选 WNOHANG 探活，最后阻塞 wait4 兜底 |
| execlp 失败了用什么退出 | `_exit` 不是 `exit`，避免刷 stdio 缓冲污染父进程 |
| setrlimit 在哪调用 | fork 后 exec 前的子进程里，影响 exec 后的用户程序 |
| select vs poll vs epoll | select 位图 1024 限、每轮重设；poll 用数组无 fd 上限；epoll 内核维护就绪表 O(1) 返。本项目 fd 少用 select 够 |
| pipe 容量死锁问题 | 默认 64KB，输入大会阻塞 write；改进:非阻塞 fd + 多线程或 epoll |
| compiler error 的 timeout 为什么是 10s | 编译通常 < 1s，10s 留冗余防止模板膨胀编译慢。可配置 |

---

## 2.9 Round 2 作业

1. **默写一遍 fork+pipe+dup2+execl 后子进程和父进程的 fd 表**，标清每端的开闭。
2. **解释为什么 SIGKILL 和 SIGXCPU 都判 TLE，但 SIGSEGV 判 RE**——直到能脱口而出。
3. **能说清楚 5 处 unlink 的位置和意义**（TLE/RE/WA/正常 return/编译失败）。
4. **能讲 MLE 缺失的根因**（无信号+memoryKb 未赋值+ru_maxrss 没用）。
5. **能量化沙箱的安全局限**：能挡什么、不能挡什么、生产怎么补。

---

**Round 2 总结**：判题沙箱是整个项目技术含量最高的模块——`fork + execlp(g++)` 编译、`fork + execl` 运行、`pipe + dup2` 重定向、`select` 多路复用超时、`setrlimit`(RLIMIT_AS/RLIMIT_CPU) 资源限制、`wait4 + rusage` 取资源使用、`WIFEXITED/WIFSIGNALED` 判定结果。每个系统调用都有原理，安全局限与改进方向也能讲清楚。

---

## 后续轮次预告

- **Round 3** 认证全链路：bcrypt → User → AuthService → SessionManager → auth_handler Cookie 三件套
- **Round 4** 数据层：连接池 + 三个 Model + 事务/sql注入/utf8mb4
- **Round 5** 前端架构与 api.js / problem_detail.js / Ace 时序 / 登录态双源
- **Round 6** 测试体系 + 模拟面试问答