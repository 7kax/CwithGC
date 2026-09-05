include_directories(${CMAKE_SOURCE_DIR}/include)

set(TEST_NAMES
    uninit_ptr
    mem_leak1
    mem_leak2
    mem_leak3
    alloc_fail
    dangling_ptr
    double_free
    use_after_free
)

set(GC_ALGORITHMS
    mark_sweep
    copying
    ref_count
)

foreach(TEST_NAME ${TEST_NAMES})
    set(TEST_FILE ${CMAKE_CURRENT_SOURCE_DIR}/${TEST_NAME}.c)
    if(EXISTS ${TEST_FILE})
        foreach(GC_ALGORITHM ${GC_ALGORITHMS})
            set(EXECUTABLE_NAME ${GC_ALGORITHM}_${TEST_NAME})
            add_executable(${EXECUTABLE_NAME} ${TEST_FILE})
            target_link_libraries(${EXECUTABLE_NAME} gc_${GC_ALGORITHM})
            add_test(NAME ${EXECUTABLE_NAME} COMMAND ${EXECUTABLE_NAME})
        endforeach()
    endif()
endforeach()

add_executable(mark_sweep_full_heap_alloc_fail full_heap_alloc_fail.c)
target_link_libraries(mark_sweep_full_heap_alloc_fail gc_mark_sweep)
add_test(NAME mark_sweep_full_heap_alloc_fail COMMAND mark_sweep_full_heap_alloc_fail)
set_tests_properties(mark_sweep_full_heap_alloc_fail PROPERTIES
    PASS_REGULAR_EXPRESSION "Allocation failed"
)

foreach(GC_ALGORITHM ${GC_ALGORITHMS})
    foreach(TEST_NAME oversized_alloc overflowing_ptr_table)
        set(EXECUTABLE_NAME ${GC_ALGORITHM}_${TEST_NAME})
        add_executable(${EXECUTABLE_NAME} ${TEST_NAME}.c)
        target_link_libraries(${EXECUTABLE_NAME} gc_${GC_ALGORITHM})
        add_test(NAME ${EXECUTABLE_NAME} COMMAND ${EXECUTABLE_NAME})
    endforeach()

    set_tests_properties(${GC_ALGORITHM}_oversized_alloc PROPERTIES
        PASS_REGULAR_EXPRESSION "Allocation fail"
    )
    set_tests_properties(${GC_ALGORITHM}_overflowing_ptr_table PROPERTIES
        PASS_REGULAR_EXPRESSION "Invalid pointer table"
    )
endforeach()
