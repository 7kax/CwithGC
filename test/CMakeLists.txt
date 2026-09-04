# 添加测试目录
include_directories(${CMAKE_SOURCE_DIR}/include)

# 测试名称列表
set(TEST_NAMES
    basic
    func_call
    recursion
    linked_list
    graph
)

# 垃圾回收算法列表
set(GC_ALGORITHMS
    mark_sweep
    copying
    ref_count
)

# 为每个算法创建测试
foreach(GC_ALGORITHM ${GC_ALGORITHMS})
    foreach(TEST_NAME ${TEST_NAMES})
        set(TEST_FILE ${CMAKE_CURRENT_SOURCE_DIR}/${GC_ALGORITHM}/${TEST_NAME}.c)
        if(EXISTS ${TEST_FILE})
            add_executable(${GC_ALGORITHM}_${TEST_NAME} ${GC_ALGORITHM}/${TEST_NAME}.c)
            target_link_libraries(${GC_ALGORITHM}_${TEST_NAME} gc_${GC_ALGORITHM})
            add_test(NAME ${GC_ALGORITHM}_${TEST_NAME} COMMAND ${GC_ALGORITHM}_${TEST_NAME})
        endif()
    endforeach()
endforeach()

# Shared allocation-contract tests run against every collector.
foreach(GC_ALGORITHM ${GC_ALGORITHMS})
    add_executable(${GC_ALGORITHM}_alignment alignment.c)
    target_link_libraries(${GC_ALGORITHM}_alignment gc_${GC_ALGORITHM})
    add_test(NAME ${GC_ALGORITHM}_alignment COMMAND ${GC_ALGORITHM}_alignment)
endforeach()
