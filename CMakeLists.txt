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
# 数据库核心模型库
add_library(minidbms_core
    src/core/value.cpp
    src/core/column.cpp
    src/core/row.cpp
    src/core/table.cpp
    src/core/database.cpp
    src/core/catalog.cpp
    src/storage/pager.cpp
)

# 允许链接 minidbms_core 的目标使用形如 "core/value.hpp" 的包含路径
target_include_directories(minidbms_core
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src
)

add_executable(server src/server_main.cpp)
target_link_libraries(server PRIVATE minidbms_core)

add_executable(client src/client_main.cpp)
