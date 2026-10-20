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
    src/storage/serializer.cpp
    src/storage/table_file.cpp
    src/storage/b_plus_tree.cpp
    src/sql/tokenizer.cpp
    src/sql/parser.cpp
    src/execution/executor.cpp
)
# 数据库网络库
add_library(minidbms_net
    src/net/socket.cpp
    src/net/io.cpp
    src/net/json.cpp
    src/net/server.cpp
    src/net/client.cpp
)

# 允许链接 minidbms_core 的目标使用形如 "core/value.hpp" 的包含路径
target_include_directories(minidbms_core
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_include_directories(minidbms_net
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src
)

find_package(Threads REQUIRED)
add_executable(server src/server_main.cpp)
target_link_libraries(server PRIVATE
    minidbms_core
    minidbms_net
    Threads::Threads
)
find_path(READLINE_INCLUDE_DIR
    NAMES readline/readline.h
    REQUIRED
)
find_library(READLINE_LIBRARY
    NAMES readline
    REQUIRED
)
add_executable(client src/client_main.cpp)
target_include_directories(client
    PRIVATE ${READLINE_INCLUDE_DIR}
)
target_link_libraries(client PRIVATE
    minidbms_net
    ${READLINE_LIBRARY}
)
