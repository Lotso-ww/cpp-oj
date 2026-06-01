# C++ Online Judge System

教学/训练用 C++ 在线评测系统。

## 功能特性

- 题目列表与详情查看
- 在线代码编辑与提交
- C++ 代码编译执行
- 测试结果判定 (AC/WA/TLE/RE)
- 用户注册登录
- 题目管理后台

## 项目结构

```
cpp-oj/
├── config/config.yaml       # 配置文件
├── database/init.sql         # 数据库初始化
├── src/                      # 后端源码
├── public/                    # 前端页面
└── tests/                    # 测试代码
```

## 依赖

- C++17
- MySQL 8.0
- cpp-httplib
- libmysqlclient

## 编译

```bash
mkdir build && cd build
cmake ..
make
```

## 运行

```bash
./bin/cpp-oj
```

## API

| 方法 | 路径 | 说明 |
|------|------|------|
| GET | `/api/problems` | 题目列表 |
| GET | `/api/problems/:id` | 题目详情 |
| POST | `/api/submit` | 提交代码 |
| POST | `/api/login` | 登录 |
| POST | `/api/register` | 注册 |

详见 [SPEC.md](./SPEC.md)