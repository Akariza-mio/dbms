#!/bin/bash
# 遇到错误立即停止脚本运行
set -e

# 每次运行都重新执行 CMake 配置，确保 build 目录使用 Release 模式。
# 重新配置不会重新编译所有未发生变化的文件，但可以把已有的 Debug
# 构建目录可靠地切换为 Release。
echo "===> 配置构建..."
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release

# 1. 多核并行编译
echo "===> 正在编译..."
cmake --build build -j

# 2. 根据传入的参数决定运行什么（默认运行 server）
# 用法:
#   ./run.sh          -> 运行 server
#   ./run.sh client   -> 运行 client
#   ./run.sh build    -> 只编译，不运行
TARGET=${1:-server}

if [ "$TARGET" = "server" ]; then
    echo "===> 启动 MiniDBMS Server..."
    ./build/server
elif [ "$TARGET" = "client" ]; then
    echo "===> 启动 MiniDBMS Client..."
    ./build/client
elif [ "$TARGET" = "build" ]; then
    echo "===> 编译完成！"
else
    echo "未知参数: $TARGET"
    echo "用法: ./run.sh [server|client|build]"
    exit 1
fi
