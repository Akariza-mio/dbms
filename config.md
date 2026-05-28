# config.md

## 操作系统信息
- 操作系统：Ubuntu / Linux
- 架构：x86_64

## 编译环境
- 编译器：GCC (g++) 支持 C++20 / C++23
- 构建工具：CMake (最低版本 3.20)

## 构建项目
在项目根目录下，执行以下命令进行构建：
```bash
mkdir build
cd build
cmake ..
make
```

## 运行方式
项目构建完成后，会在 `build` 目录下生成 `server` 和 `client` 可执行文件。

1. **运行服务端**:
   ```bash
   ./build/server [port]
   ```
   *说明：服务端默认监听 `8080` 端口。*

2. **运行客户端**:
   ```bash
   ./build/client [ip] [port]
   ```
   *说明：客户端默认连接 `127.0.0.1:8080`。进入交互式 CLI 后，可以直接输入 SQL 语句进行数据库操作，输入 `exit` 退出。*

## 测试方式
您也可以运行内置的无 STL 容器测试程序：
```bash
./build/tests/test_vector
./build/tests/test_core
./build/tests/test_storage
./build/tests/test_bptree
./build/tests/test_parser
./build/tests/test_executor
```

## 存储目录说明
- 本 DBMS 所有的数据都存储在项目根目录的 `data/` 文件夹下。
- 表数据文件后缀为 `.dat`。
- B+树索引文件后缀为 `.idx`。
- 数据库元数据保存于 `data/catalog.meta` 中。
