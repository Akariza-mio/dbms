# MiniDBMS

MiniDBMS 是一个使用 C++20 实现的轻型关系型数据库。项目采用客户端/服务端（C/S）架构，包含 SQL 词法与语法分析、执行器、目录元数据、二进制行存储、主键 B+ 树索引、TCP 通信和交互式命令行客户端。

## 1. 主要功能

### 1.1 数据模型与持久化

- 支持 `INT`、`FLOAT`、`STRING` 三种字段类型。
- `STRING` 最长为 256 字节。
- 支持数据库、表、列、行和单列主键。
- 使用 `catalog.meta` 保存数据库、表、列、类型和主键信息。
- 使用自定义二进制 `.dat` 文件追加保存行记录。
- 使用墓碑标记删除行，避免移动其他记录和改变行偏移。
- 服务端重启后可以重新加载目录、表数据和索引。

### 1.2 SQL

- 数据库：`CREATE DATABASE`、`DROP DATABASE`、`USE`、`SHOW DATABASES`。
- 表：`CREATE TABLE`、`DROP TABLE`、`SHOW TABLES`。
- 数据：单行/批量 `INSERT`、`SELECT`、`UPDATE`、`DELETE`。
- 查询：`SELECT *`、单列/多列投影、`= / < / >` 条件、`AND / OR` 条件链。
- 聚合：`COUNT`、`MAX`、`MIN`、`AVG`。
- 分组：单列 `GROUP BY`。
- 表结构修改：增加列、删除非主键列、重命名列、重命名表。

### 1.3 索引与存储

- 页面大小固定为 4096 字节。
- 支持 `INT` 和 `STRING` 主键的 B+ 树索引。
- 支持等值查询、乱序插入、叶节点分裂、内部节点分裂和多层树。
- 索引根页和节点内容持久化到 `.idx` 文件。
- 支持不进行借位和合并的简单索引删除。
- 主键等值条件优先走 B+ 树点查，其他查询回退为全表扫描。

### 1.4 客户端与服务端

- 服务端默认监听 `127.0.0.1:8080`，端口可通过参数修改。
- 支持多个客户端同时连接，一条连接由一个线程处理。
- 每个客户端拥有独立的当前数据库状态，`USE` 不会影响其他连接。
- 数据库文件操作使用全局互斥锁保护，避免多个线程同时修改文件。
- 服务端使用 JSON 返回成功状态和结果信息。
- 客户端支持行编辑、上下方向键历史、`exit`、`quit` 和 EOF 退出。

## 2. 开发与运行环境

当前项目在以下环境中开发和构建：

| 项目 | 环境 |
|---|---|
| 操作系统 | Ubuntu 24.04 LTS / WSL2 |
| 编译器 | GCC / g++ 15.2.0 |
| CMake | 3.28.3，最低要求3.20 |
| C++ 标准 | C++20，关闭编译器扩展 |
| 编译警告 | `-Wall -Wextra -Wpedantic` |
| 网络接口 | Linux/POSIX Socket API |
| 线程 | C++ 标准线程及 CMake `Threads::Threads` |

在其他较新的 Linux 发行版上，只要编译器完整支持 C++20，并安装 CMake 和 GNU Readline，也可以进行构建。

## 3. 技术栈

- C++20
- CMake
- POSIX Socket
- GNU Readline
- 自定义 SQL Tokenizer、Parser 和 AST
- 自定义二进制序列化、行文件、分页器和 B+ 树
- 自定义最小 JSON 编码与解析

## 4. 代码结构与说明

```text
MiniDBMS/
├── CMakeLists.txt              # 构建核心库、网络库、server 和 client
├── README.md                   # 项目说明和运行文档
├── run.sh                      # 编译并启动 server/client 的辅助脚本
├── data/                       # 运行时生成的数据库文件，不提交到 Git
└── src/
    ├── core/                   # 类型、值、行、表、数据库和目录元数据
    ├── storage/                # 分页器、序列化、TableFile 和 B+ 树
    ├── sql/                    # Token、Tokenizer、AST 和 Parser
    ├── execution/              # SQL 执行器和查询/修改逻辑
    ├── net/                    # Socket RAII、TCP I/O、JSON 和连接辅助函数
    ├── server_main.cpp         # 服务端入口、多客户端线程和结果格式化
    └── client_main.cpp         # 客户端入口、Readline 交互循环
```
各模块的职责如下：

| 模块 | 主要职责 |
|---|---|
| `core` | 表示 `Value`、`Column`、`Row`、`Table`、`Database` 和 `Catalog` 等数据库对象。 |
| `storage` | 把行和索引转换为二进制数据，负责文件偏移、页面读写、墓碑删除和 B+ 树维护。 |
| `sql` | 把 SQL 文本切分为 Token，再解析为与存储无关的 `SQLCommand`。 |
| `execution` | 校验数据库状态和 Schema，选择索引或全表扫描，并协调目录、数据文件和索引文件。 |
| `net` | 封装文件描述符、连接建立、完整发送、按行接收和 JSON 响应。 |
| `server_main.cpp` | 接收客户端，维护每连接会话，并把 Parser、Executor 和网络层连接起来。 |
| `client_main.cpp` | 读取用户输入、发送 SQL、接收 JSON，并以终端表格显示查询结果。 |

## 5.数据库的通信流程

```txt
客户端输入 SQL
   ↓
客户端把 SQL 发送到 TCP 连接
   ↓
服务端接收完整 SQL
   ↓
Parser → Executor
   ↓
服务端把执行结果转换为 JSON
   ↓
服务端通过 TCP 发回 JSON
   ↓
客户端解析并显示结果
```

网络层只负责在两个进程之间传递“SQL 请求”和“执行结果”。

本项目采用换行符分帧:服务端不断接收，将字节放进缓冲区。只有找到真正的换行符，才取出前面的内容作为一条完整 SQL。

为什么响应使用JSON：

服务端不能只返回一句随意的文字，因为客户端需要知道

1. SQL是否执行成功
2. 成功或失败的消息是什么

最低限度的响应格式：

```JSON
{"ok":true,"msg":"database created"}
```

JSON是应用层数据格式，决定字节表达什么

## 6. 系统逻辑架构

### 6.1 一条 SQL 的完整执行流程

```text
用户输入 SQL
    ↓
client：Readline 读取命令
    ↓
TCP：发送以换行符结尾的 SQL
    ↓
server：LineReader 拼接出一条完整消息
    ↓
Tokenizer：SQL 文本 → Token 序列
    ↓
Parser：Token 序列 → SQLCommand
    ↓
Executor：校验会话、Schema 和数据类型
    ↓
Catalog / TableFile / BPlusTree：读取或修改持久化文件
    ↓
ExecutionResult：生成列名、结果行、消息和影响行数
    ↓
server：结果格式化 → JSON → TCP
    ↓
client：解析 JSON 并输出到终端
```

Tokenizer 和 Parser 只理解 SQL 语法，不读取数据库文件；Executor 才负责把语法命令转换为实际的数据库操作。这样的分层避免 SQL 解析、业务规则和文件格式互相混杂。

### 6.2 查询流程

```text
SELECT
  ├─ WHERE 中包含“主键 = 常量”且没有 OR
  │      ↓
  │   B+ 树查到行偏移
  │      ↓
  │   TableFile 按偏移读取目标行
  │
  └─ 其他条件或无主键
         ↓
      TableFile 扫描全部活动行

候选行 → WHERE 条件过滤 → 投影或聚合/GROUP BY → ExecutionResult
```

B+ 树只保存“主键到 `.dat` 行偏移”的映射，完整行仍然保存在表数据文件中。索引查询获得偏移后，还要回到 `.dat` 文件读取实际记录。

### 6.3 写入、更新与删除流程

- `INSERT`：支持 `VALUES (...), (...)` 批量写入；先检查整批数据的列数、类型和主键是否重复，再向 `.dat` 追加行；有主键时把主键和新行偏移写入 `.idx`。批次写入失败时会撤销本批已经插入的索引键，并用墓碑隐藏本批已经追加的行。
- `DELETE`：将 `.dat` 中的行标记为墓碑，并从主键索引删除对应键；不移动其他行。
- `UPDATE`：先检查类型和新主键冲突，再追加修改后的新行、更新索引，最后将旧行标记为删除。
- `ALTER TABLE`：需要改变行布局时创建临时数据文件，重写活动行并重建索引，再替换原文件。
- 目录变更：先在内存中修改 `Catalog`，保存到临时元数据文件后再通过重命名替换 `catalog.meta`。

跨 `.dat`、`.idx` 和 `catalog.meta` 的操作包含基本的失败回滚，但项目没有实现事务日志，因此不提供数据库级 ACID 事务或进程崩溃恢复保证。

### 6.4 多客户端流程

```text
监听 Socket
    ↓ accept
客户端 Socket ──→ 独立线程 ──→ 独立 Executor / 当前数据库
                              ↓
                       全局数据库互斥锁
                              ↓
                   共享 catalog/.dat/.idx 文件
```

锁只包围数据库命令执行，不包围客户端输入、SQL 解析和网络发送。因此多个客户端可以同时保持连接和输入命令，但共享文件修改会依次执行。当前实现采用较粗的全局锁，优先保证文件一致性。

## 7. 运行时数据结构

服务端从哪个目录启动，就在该目录下使用 `data/`：

```text
data/
├── catalog.meta
└── <database>/
    ├── <table>.dat
    └── <table>.idx       # 只有带主键的表才存在
```

- `catalog.meta`：文本元数据，包含格式版本、数据库、表、列、类型和主键标志。
- `.dat`：二进制行文件，记录活动/删除标志、负载长度和序列化后的字段。
- `.idx`：以4096字节固定页组织的 B+ 树文件，第0页保存索引元数据和根页号。

`data/` 已被 `.gitignore` 忽略。
### 7.1 领域关系模型

当前数据的领域关系模型：

```text
Catalog
└── vector<Database>
    └── vector<Table>
        └── vector<Column>

Row
└── vector<Value>
```

### 7.2 catalog的文件格式

以下面的格式为例：

```text
MINIDBMS_CATALOG 1 //表示格式版本
DATABASE_COUNT 1 //数据库数量
DATABASE school 1 //名为school的数据库中有一个表
TABLE student 4 //在school这个数据库中的名为student的表，它有4列
COLUMN id INT 1 //student表中名为id的列是主键，类型为INT
COLUMN name STRING 0
COLUMN age INT 0
COLUMN score FLOAT 0
END_TABLE //结束标记
END_DATABASE
END_CATALOG
```
- 行文件的格式：

一条记录：

```text
┌──────────────┬────────────────┬────────────────────┐
│ deleted: 1B  │ payload_size:4B│ 序列化后的Row数据    │
└──────────────┴────────────────┴────────────────────┘
```

delete表示删除，0或1.

payload_size表示后面的Row数据占多少字节。

Row数据：列数量和所有value.

Row的格式：

```text
┌─────────────────┬─────────┬─────────┬─────┐
│ value_count: 4B │ Value 0 │ Value 1 │ ... │
└─────────────────┴─────────┴─────────┴─────┘
```

每个Value：

```text
INT:
┌─────────┬──────────────┐
│ type:1B │ int32数据:4B │
└─────────┴──────────────┘

FLOAT:
┌─────────┬──────────────┐
│ type:1B │ double数据:8B│
└─────────┴──────────────┘

STRING:
┌─────────┬───────────┬────────────────┐
│ type:1B │ length:4B │ length个字符字节│
└─────────┴───────────┴────────────────┘
```

原子保存：

不覆盖正式文件，而是先生成临时文件，检查写入和关闭是否成功，再重命名为catalog.meta

## 8. 部署与运行

### 8.1 安装依赖

Ubuntu / WSL2：

```bash
sudo apt update
sudo apt install build-essential cmake libreadline-dev
```

### 8.2 配置与编译

在项目根目录执行：

```bash
./run.sh
```

生成的程序为：

```text
build/server
build/client
```

### 8.3 启动服务端

默认监听 `127.0.0.1:8080`：

```bash
./build/server
```

指定端口：

```bash
./build/server 9000
```

服务端使用当前工作目录下的 `data/`。建议始终从项目根目录启动。当前没有专门的服务端关闭命令，应在没有 SQL 正在执行时按 `Ctrl+C` 停止。

### 8.4 启动客户端

另开一个 WSL 终端，连接默认地址和端口：

```bash
./build/client
```

指定地址和端口：

```bash
./build/client 127.0.0.1 9000
```

客户端中输入 `exit`、`quit` 或按 `Ctrl+D` 可以退出当前连接，不会关闭服务端。

### 8.5 简单示例

```sql
CREATE DATABASE school;
USE school;

CREATE TABLE students (
    id INT PRIMARY,
    name STRING,
    score FLOAT
);

INSERT INTO students VALUES (1, 'Alice', 92.5);
INSERT INTO students VALUES
    (2, 'Bob', 80),
    (3, 'Carol', 87.5);

SELECT * FROM students;
SELECT name, score FROM students WHERE id = 1;
SELECT count(id), avg(score) FROM students;

UPDATE students SET score = 95.0 WHERE id = 1;
DELETE FROM students WHERE id = 2;
```

## 9. 第三方资源使用说明

### GNU Readline

客户端使用系统安装的 GNU Readline 提供行编辑和历史记录功能，对应源码位于 `src/client_main.cpp`，构建时由 CMake 查找：

```cmake
find_path(READLINE_INCLUDE_DIR NAMES readline/readline.h REQUIRED)
find_library(READLINE_LIBRARY NAMES readline REQUIRED)
```

GNU Readline 采用 **GNU General Public License version 3 or later（GPL-3+）**。本仓库没有复制或修改 Readline 源码，而是链接系统提供的库；分发包含该依赖的程序时，需要自行确认并遵守其许可证要求。本地 Ubuntu 包的完整版权说明通常位于：

```text
/usr/share/doc/libreadline-dev/copyright
```

### 系统库与项目内实现

- 线程支持来自 C++ 标准库和系统线程库，通过 CMake `Threads::Threads` 链接。
- Socket、文件和文件系统功能来自 Linux/POSIX API 与 C++ 标准库。
- JSON、SQL Tokenizer/Parser、序列化、Pager、TableFile 和 B+ 树均为本项目自行实现，没有引入第三方数据库、SQL 解析器或 JSON 库。

