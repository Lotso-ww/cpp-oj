# OJ 系统依赖安装指南

本文档适用于空白 Ubuntu 24.04 系统环境。

## 1. 系统基础依赖

```bash
sudo apt update
sudo apt install -y build-essential g++ cmake
```

| 包名 | 说明 |
|------|------|
| build-essential | GCC 编译工具链，包含 make、gcc 等 |
| g++ | C++ 编译器，用于编译 OJ 后端代码 |
| cmake | CMake 构建工具 |

## 2. 数据库

```bash
sudo apt install -y mysql-server libmysqlclient-dev
```

| 包名 | 说明 |
|------|------|
| mysql-server | MySQL 数据库服务器 |
| libmysqlclient-dev | MySQL 客户端开发库，用于 C++ 连接 MySQL |

**或使用 Docker 运行 MySQL：**
```bash
sudo apt install -y docker.io docker-compose
docker run -d --name mysql -p3306:3306 -e MYSQL_ROOT_PASSWORD=your_password mysql:8.0
```

## 3. cpp-httplib 依赖

```bash
sudo apt install -y libssl-dev
```

| 包名 | 说明 |
|------|------|
| libssl-dev | OpenSSL 开发库，cpp-httplib  HTTPS 支持需要 |

> cpp-httplib 本身是单头文件库，通过 CMakeLists.txt 引入，无需单独安装。

## 4. SPJ 特判脚本依赖

```bash
sudo apt install -y python3
```

| 包名 | 说明 |
|------|------|
| python3 | Python 3 解释器，用于执行 SPJ 特判校验脚本 |

## 5. 测试框架（可选）

```bash
sudo apt install -y libgtest-dev libgmock-dev
```

| 包名 | 说明 |
|------|------|
| libgtest-dev | Google Test 测试框架 |
| libgmock-dev | Google Mock模拟框架 |

## 6. 一键安装脚本

```bash
#!/bin/bash
set -e

sudo apt update
sudo apt install -y build-essential g++ cmake mysql-server libmysqlclient-dev libssl-dev python3 libgtest-dev libgmock-dev docker.io docker-compose
```

## 7. 验证安装

```bash
g++ --version      # C++ 编译器
cmake --version    # CMake
mysql --version    # MySQL 客户端
python3 --version  # Python 3
openssl version # OpenSSL
```