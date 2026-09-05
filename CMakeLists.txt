cmake_minimum_required(VERSION 3.10)
project(CwithGC LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 启用测试
enable_testing()

# 添加库目录
add_subdirectory(src)

# 添加测试目录
add_subdirectory(test)

# 添加验证目录
add_subdirectory(verify)
