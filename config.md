# MiniDBMS 项目配置与运行指南

本项目是一个基于现代 C++ 从零构建的**微型关系型数据库管理系统 (MiniDBMS)**。它没有依赖庞大的第三方存储库或标准模板库 (STL) 的核心组件，而是完全自主实现了从底层存储、B+树索引、SQL 解析到 C/S 网络通信的全栈架构。

---

## 🌟 核心特性 (Core Features)

1. **Client-Server 架构**：通过原生的 Socket TCP 和多线程技术，支持多客户端同时接入并发操作。内置了线程互斥锁保证底层文件系统读写安全。
2. **纯自研底层引擎**：
   - **核心结构**：自主实现了轻量级的 `Vector` 动态数组与 `String` 字符串类，脱离了对 `std::vector` 的重度依赖。
   - **存储引擎**：基于页（Page）的行式存储系统，将数据通过二进制格式（`.dat`）紧凑序列化到本地硬盘。
   - **索引机制**：实现了基于磁盘的 **B+树（BPlusTree）** 索引引擎，为表的主键提供高速的点查能力。
3. **丰富的 SQL 支持**：
   - **数据定义 (DDL)**：支持 `CREATE DATABASE`, `CREATE TABLE`，以及强大的 `ALTER TABLE`（支持增删列、重命名表、重命名列）。
   - **数据操作 (DML)**：支持 `INSERT`, `DELETE`, `UPDATE`。
   - **数据查询 (DQL)**：支持多条件 `WHERE`，支持 `FLOAT` 等类型，具备 `COUNT/MAX/MIN/AVG` 四大聚合函数，并完美支持基于指定列的 `GROUP BY` 分组汇总。

---

## ⚙️ 运行环境配置 (Environment Setup)

在部署并运行本项目之前，请确保您的计算机满足以下环境要求：

- **操作系统**：Linux (推荐 Ubuntu 20.04 或以上版本)，支持 POSIX 标准的系统。
- **处理器架构**：x86_64 或 ARM64。
- **编译器**：GCC (g++)，需要支持 **C++20** 或更高标准。
- **构建工具**：CMake (最低版本要求 3.20)。
- **网络配置**：本地开发无需互联网连接，通过系统的回环网络（Loopback `127.0.0.1`）即可运行。默认需确保 `8080` 端口未被其他程序占用。

---

## 📁 项目目录结构 (Project Structure)

本数据库严格遵循模块化设计原则：

```text
/~/dbms
├── build/                # CMake 编译生成的构建目录（可执行文件位于此）
├── data/                 # 数据库落盘文件目录
│   ├── catalog.meta      # 整个系统的元数据（记录了有哪些库和表）
│   ├── *.dat             # 表的二进制行数据存储文件
│   └── *.idx             # B+树主键索引文件
├── src/                  # 核心源代码
│   ├── core/             # 基础支撑层：Types.h, Vector.h, String.h
│   ├── storage/          # 存储层：Pager, TableFile, BPlusTree
│   ├── sql/              # 逻辑层：Parser 解析器, Executor 执行引擎, AST 抽象语法树
│   └── server_main.cpp   # 服务端主入口（监听端口、分发线程）
├── client/               # 客户端源码
│   └── client_main.cpp   # 交互式 CLI 客户端入口
├── tests/                # 单元测试与集成测试模块
├── CMakeLists.txt        # 项目构建脚本
└── config.md             # 本项目的配置与说明文档
```

---

## 本地部署与运行方式 (Deployment)

### 1. 编译构建
进入项目根目录，通过 CMake 生成 Makefile 并执行构建：
```bash
mkdir -p build && cd build
cmake ..
make -j4
```
构建完成后，会在 `build` 目录下生成 `server`、`client` 两个主程序，以及 `tests/` 目录下的相关测试用例程序。

### 2. 启动服务端 (Server)
服务端进程是整个 DBMS 的大脑，负责处理数据和管理文件。
```bash
cd build
./server 8080
```
- **说明**：服务端将在前台挂起并默认监听 `8080` 端口。`8080` 可替换为您想要的任何端口号。

### 3. 启动客户端 (Client)
打开一个**新的终端窗口**，运行客户端程序去连接服务端：
```bash
cd build
./client 127.0.0.1 8080
```
- **说明**：参数一为服务端的 IP（`127.0.0.1` 表示连接本机的服务端），参数二为端口号。连接成功后，您将进入 `MiniDBMS >` 交互界面。

---

## 💡 基础交互示例 (Quick Start)

在客户端连入服务端后，您可以尝试执行以下一套完整的业务流测试：

```sql
-- 1. 创建并切换数据库
create database testdb
use testdb

-- 2. 创建带主键的表
create table student (id int primary, name string, score float)

-- 3. 插入多条包含浮点数的数据
insert student values (1, "Alice", 95.5)
insert student values (2, "Bob", 88.0)
insert student values (3, "Cindy", 95.5)

-- 4. 执行聚合与分组查询 (计算各分数的同分人数)
select score, count(id) from student group by score

-- 5. 修改表结构 (重命名表)
alter table student rename to pupils

-- 6. 安全退出
exit
```

---

## 🧪 测试说明

如需对数据库底层结构进行二次开发，我们在项目中内置了一套完备的测试驱动模块。您可以在 `build` 目录下单独运行：
- `./tests/test_core`：验证自定义 `Vector` 和 `String` 的内存正确性。
- `./tests/test_storage`：验证底层的二进制落盘与页管理逻辑。
- `./tests/test_parser` 和 `./tests/test_executor`：验证 SQL 解析的边界情况与业务执行正确性。
