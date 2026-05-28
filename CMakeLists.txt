cmake_minimum_required(VERSION 3.20)
project(MiniDBMS VERSION 1.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Enable warnings
add_compile_options(-Wall -Wextra -Wpedantic)

# Include directories
include_directories(src)

# --- Executables ---

# Server executable
add_executable(server src/server_main.cpp)

# Client executable
add_executable(client src/client_main.cpp src/linenoise.c)

# --- Testing ---
enable_testing()
add_subdirectory(tests)
