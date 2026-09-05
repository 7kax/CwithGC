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
add_library(gc_pointer_table OBJECT src/gc_ptr_table.cpp)
add_library(gc_mark_sweep STATIC src/mark_sweep/mark_sweep.cpp $<TARGET_OBJECTS:gc_pointer_table>)
add_library(gc_copying STATIC src/copying/copying.cpp $<TARGET_OBJECTS:gc_pointer_table>)
add_library(gc_ref_count STATIC src/ref_count/ref_count.cpp $<TARGET_OBJECTS:gc_pointer_table>)

# 添加测试目录
add_subdirectory(test)

# 添加验证目录
add_subdirectory(verify)
