cmake_minimum_required(VERSION 3.10)
project(CwithGC LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 启用测试
enable_testing()

# 添加头文件目录
include_directories(include)

# 创建库文件
# 每种算法对应一个库
add_library(gc_mark_sweep STATIC src/mark_sweep/mark_sweep.cpp)

# 添加测试目录
add_subdirectory(test)

# 创建示例程序
# file(GLOB_RECURSE EXAMPLE_SOURCES "src/main.c")
# add_executable(${PROJECT_NAME} ${EXAMPLE_SOURCES})

# 设置要链接的库
# target_link_libraries(${PROJECT_NAME} gc_mark_sweep)