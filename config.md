# MiniDBMS 开发环境与构建配置

## 1. 运行与开发环境

- **操作系统**: Ubuntu 24.04 LTS (Linux / WSL2)
- **编译器**: GCC / g++ 15.2.0 (完全支持 C++20 / C++23)
- **构建工具**: CMake 3.28.3
- **编译标准**: C++20 (`-std=c++20`, `CMAKE_CXX_EXTENSIONS OFF`)
- **警告选项**: `-Wall -Wextra -Wpedantic`

---

## 2. 核心构建与运行命令

### 2.1 配置工程 (Debug 模式)
```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
```

### 2.2 编译工程
```bash
cmake --build build -j
```

### 2.3 运行可执行程序
```bash
# 服务端
./build/minidbms_server

# 客户端
./build/minidbms_client
```

### 2.4 运行所有自动化测试 (CTest)
```bash
ctest --test-dir build --output-on-failure
```
