# 部署指南

本文档面向运维与后端开发人员，给出从零环境到 OJ 服务在线运行的完整、可复现步骤。目标平台为 Ubuntu 22.04 / 24.04 LTS，理论上兼容任何满足依赖矩阵的 Linux 发行版。

## 一、依赖与环境矩阵

| 维度 | 要求 | 说明 |
|------|------|------|
| 操作系统 | Ubuntu 22.04 / 24.04 LTS（x86_64） | 其他发行版需自行替换包管理器命令 |
| 编译器 | g++ >= 9，支持 C++17 | 推荐 GCC 11 / GCC 13 |
| 构建工具 | CMake >= 3.10 | 同时需要 `make` |
| 数据库 | MySQL 8.0 | 字符集 `utf8mb4`，引擎 InnoDB |
| 数据库客户端 | `libmysqlclient-dev` | 提供 `mysql.h` 与 `mysqlclient` 链接库 |
| HTTP 框架 | cpp-httplib（单头文件，已随仓库分发） | 无需独立安装 |
| 配置库 | yaml-cpp | 通过 `pkg-config yaml-cpp` 解析 |
| JSON 库 | jsoncpp（`/usr/include/jsoncpp`） | 请求体与响应体序列化 |
| 加密库 | OpenSSL >= 1.1 | 会话签名；启用 HTTPS 时必须 |
| 密码哈希 | Botan 2（`/usr/include/botan-2`） | 用户口令 bcrypt |
| 判题编译器 | g++（运行时调用） | 用于编译用户提交的 C++ 代码 |
| 单元测试框架（可选） | GoogleTest / GoogleMock | 执行 `tests/unit` |
| 接口测试工具（可选） | curl、bash | 执行 `tests/integration` |
| Web 端到端测试（可选） | Node.js >= 18、npx、Playwright CLI | 执行前端自动化用例，无需预装系统浏览器 |
| 前端运行时依赖 | 任意可联网浏览器 | 自动从 jsDelivr / cdnjs 拉取 Ace、marked、霞鹜文楷 |

## 二、配置参数详解

配置文件位于 `config/config.yaml`，由 `src/utils/config.cc` 加载，应用启动时一次性读入内存。

```yaml
database:
  host: "localhost"        # MySQL 监听地址
  port: 3306               # MySQL 端口
  username: "lotso"        # 数据库账号（推荐使用 auth_socket 插件认证）
  password: ""             # 密码（auth_socket 模式下留空）
  name: "oj_system"        # 库名，须与 init.sql 一致

server:
  host: "0.0.0.0"          # HTTP 监听地址，0.0.0.0 表示全部网卡
  port: 8080               # HTTP 监听端口

connection_pool:
  size: 10                 # MySQL 连接池容量，承载并发判题与后台写操作

timeouts:
  request: 5000            # HTTP 请求总超时（毫秒）
  compile: 10000           # 用户代码编译超时（毫秒）
  run: 5000                # 用户代码运行墙钟超时（毫秒）

logging:
  level: "info"            # 日志级别：debug / info / warn / error
  file: "oj.log"           # 日志文件路径，相对进程工作目录
```

字段语义补充：

- `database.username` 若使用 `auth_socket` 插件认证，`password` 必须为空，且运行进程的 Linux 账号须与数据库账号同名。
- `timeouts.run` 即判题 TLE 阈值，超过即向客户端返回 `TLE` 而非等待。
- `connection_pool.size` 建议不低于预期并发判题数；判题执行涉及多次数据库访问（拉取用例、写回判定结果）。
- 前端运行时依赖的 CDN（`cdn.jsdelivr.net`、`cdnjs.cloudflare.com`）在生产部署中如处于隔离网络，需在内网建立镜像或在 HTML 中将外链替换为本地副本。

## 三、构建与启动流程

### 3.1 安装系统依赖

```bash
sudo apt update
sudo apt install -y \
    build-essential \
    g++ \
    cmake \
    mysql-server \
    libmysqlclient-dev \
    libssl-dev \
    python3 \
    libgtest-dev \
    libgmock-dev \
    libyaml-cpp-dev \
    libjsoncpp-dev \
    libbotan-2-dev \
    curl
```

### 3.2 初始化 MySQL

```bash
sudo systemctl enable --now mysql
sudo mysql -e "CREATE USER IF NOT EXISTS 'lotso'@'localhost' IDENTIFIED WITH auth_socket; GRANT ALL ON *.* TO 'lotso'@'localhost';"
mysql -u lotso < database/init.sql
mysql -u lotso oj_system -e "SHOW TABLES;"
```

预期输出包含 `problems`、`test_cases`、`users` 三张表。

### 3.3 创建管理员账号

```bash
# 进入交互式创建，按提示输入用户名与密码
./build/bin/create_admin
```

执行产物路径取决于 CMake 运行目录，若使用第 3.4 节的标准构建命令，产物位于 `build/bin/create_admin`。

### 3.4 编译后端

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"
```

构建完成后，可执行文件位于 `build/bin/cpp-oj`。

### 3.5 启动服务

```bash
./build/bin/cpp-oj
```

启动日志示意：

```text
[INFO] Database connection pool initialized, size=10
[INFO] HTTP server listening on 0.0.0.0:8080
```

### 3.6 验证服务可用

```bash
# 根路径静态资源
curl -I http://localhost:8080/index.html

# 题目列表
curl http://localhost:8080/api/problems

# 登录获取 Cookie
curl -c cookies.txt -X POST http://localhost:8080/api/login \
    -H 'Content-Type: application/json' \
    -d '{"username":"<your_admin>","password":"<your_password>"}'
```

### 3.7 运行测试（可选）

```bash
# C++ 单元与集成测试
cd build
ctest --output-on-failure
```

Web 端到端测试依赖 Node.js 18+，建议另起终端：

```bash
# 校验 Node.js 与 npx
node -v
npx -v

# 触发 Playwright CLI（首次执行会自动按需拉取依赖）
npx --no-install playwright-cli --help
```

### 3.8 端口与放行

服务默认监听 `8080`，如部署在云主机或容器中，需在安全组 / iptables 中放行：

```bash
sudo ufw allow 8080/tcp
```

## 四、常见异常排查

### 4.1 CMake 阶段找不到 `mysqlclient`

**现象**：

```text
Could NOT find mysqlclient (missing: mysqlclient_INCLUDE_DIRS mysqlclient_LIBRARIES)
```

**根因**：未安装 MySQL 客户端开发头文件。`CMakeLists.txt` 通过 `pkg-config mysqlclient` 解析依赖。

**解决**：

```bash
sudo apt install -y libmysqlclient-dev pkg-config
pkg-config --exists mysqlclient && echo OK || echo MISSING
```

### 4.2 启动时报 `Access denied for user 'lotso'@'localhost'`

**现象**：

```text
[ERROR] Failed to connect to MySQL: Access denied for user 'lotso'@'localhost'
```

**根因**：运行进程的 Linux 账号与数据库账号不匹配，或未启用 `auth_socket` 插件。

**解决**：确认当前 Shell 用户为 `lotso`（与 `config.yaml` 中 `username` 一致），并重新创建数据库账号：

```bash
sudo mysql -e "DROP USER IF EXISTS 'lotso'@'localhost';"
sudo mysql -e "CREATE USER 'lotso'@'localhost' IDENTIFIED WITH auth_socket; GRANT ALL ON *.* TO 'lotso'@'localhost';"
```

若部署为非 `auth_socket` 模式，将 `config.yaml` 中 `password` 字段填入真实口令，并将 `IDENTIFIED WITH auth_socket` 替换为 `IDENTIFIED BY '<your_password>'`。

### 4.3 判题阶段频繁返回 CE 且提示找不到 `g++`

**现象**：用户代码提交后立即返回 `Compilation Error`，后端日志显示 `g++: command not found`。

**根因**：运行环境镜像为最小化安装，未携带 C++ 编译器。

**解决**：

```bash
sudo apt install -y g++
which g++
```

### 4.4 判题长时间无响应或进程残留

**现象**：HTTP 请求超过 `timeouts.run` 后才返回 `TLE`，且工作目录留下临时编译产物。

**根因**：用户代码进入死循环、阻塞 IO 或耗尽内存，未被 `setrlimit` 与墙钟监控及时终止。

**解决**：

1. 检查 `config.yaml` 中 `timeouts.run` 是否合理（默认 5000 ms）。
2. 确认执行器在派生子进程后正确接管 `SIGKILL` 路径；如已重启服务仍有残留，需手动清理：

```bash
pkill -9 -f '/tmp/cpp-oj-runner' || true
```

### 4.5 前端页面编辑器空白或样式缺失

**现象**：登录或详情页中代码编辑框无高亮、字体回退到系统默认，或 Markdown 描述显示为原始文本。

**根因**：浏览器无法访问 `cdn.jsdelivr.net` / `cdnjs.cloudflare.com`，导致 Ace、marked、霞鹜文楷等 CDN 资源加载失败。

**解决**：

1. 在浏览器开发者工具的 Network 面板确认资源是否 200。
2. 若处于离线 / 内网环境，将以下资源下载至 `public/vendor/` 并在 HTML 中将外链替换为相对路径：

```text
public/vendor/ace/1.23.4/ace.js
public/vendor/ace/1.23.4/mode-c_cpp.js
public/vendor/marked/12.0.0/marked.min.js
public/vendor/lxgw-wenkai-webfont/1.7.0/style.css
public/vendor/lxgw-wenkai-webfont/1.7.0/lxgwwenkai-regular.woff2
```