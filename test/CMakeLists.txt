# 定义测试源文件列表
set(TEST_SOURCES
    mark_sweep/basic.c
    mark_sweep/func_call.c
    mark_sweep/recursion.c
    mark_sweep/linked_list.c
)

# 遍历源文件创建测试
foreach(SOURCE ${TEST_SOURCES})
    get_filename_component(TEST_NAME ${SOURCE} NAME_WE)
    add_executable(${TEST_NAME} ${SOURCE})
    target_link_libraries(${TEST_NAME} gc_mark_sweep)
    add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
endforeach()
