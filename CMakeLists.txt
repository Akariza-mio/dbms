cmake_minimum_required(VERSION 3.20)
project(MiniDBMS LANGUAGES CXX)
# 显示指定标准：C++20
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
#编译警告：严格
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()
# 支持头文件根路径引入
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)

add_executable(server src/server_main.cpp)
add_executable(client src/client_main.cpp)