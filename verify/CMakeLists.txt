set(TEST_NAMES
    uninit_ptr
    dangling_ptr
    double_free
    use_after_free
)

if(GC_ENABLE_INSPECTION)
    list(APPEND TEST_NAMES
        mem_leak1
        mem_leak2
        mem_leak3
        alloc_fail
    )
endif()

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
            target_link_libraries(${EXECUTABLE_NAME} PRIVATE gc_${GC_ALGORITHM})
            gc_apply_build_options(${EXECUTABLE_NAME})
            add_test(NAME ${EXECUTABLE_NAME} COMMAND ${EXECUTABLE_NAME})
        endforeach()
    endif()
endforeach()

if(GC_ENABLE_INSPECTION)
    add_executable(mark_sweep_full_heap_alloc_fail full_heap_alloc_fail.c)
    target_link_libraries(mark_sweep_full_heap_alloc_fail PRIVATE gc_mark_sweep)
    gc_apply_build_options(mark_sweep_full_heap_alloc_fail)
    add_test(NAME mark_sweep_full_heap_alloc_fail COMMAND mark_sweep_full_heap_alloc_fail)
    set_tests_properties(mark_sweep_full_heap_alloc_fail PROPERTIES
        PASS_REGULAR_EXPRESSION "Allocation failed"
    )
endif()

foreach(GC_ALGORITHM ${GC_ALGORITHMS})
    set(EXECUTABLE_NAME ${GC_ALGORITHM}_oversized_alloc)
    add_executable(${EXECUTABLE_NAME} oversized_alloc.c)
    target_link_libraries(${EXECUTABLE_NAME} PRIVATE gc_${GC_ALGORITHM})
    gc_apply_build_options(${EXECUTABLE_NAME})
    add_test(NAME ${EXECUTABLE_NAME} COMMAND ${EXECUTABLE_NAME})

    set(PTR_TABLE_EXECUTABLE ${GC_ALGORITHM}_overflowing_ptr_table)
    add_executable(${PTR_TABLE_EXECUTABLE} overflowing_ptr_table.c)
    target_link_libraries(${PTR_TABLE_EXECUTABLE} PRIVATE gc_${GC_ALGORITHM})
    gc_apply_build_options(${PTR_TABLE_EXECUTABLE})
    add_test(NAME ${PTR_TABLE_EXECUTABLE} COMMAND ${PTR_TABLE_EXECUTABLE} overflow)

    set_tests_properties(${GC_ALGORITHM}_oversized_alloc PROPERTIES
        PASS_REGULAR_EXPRESSION "Allocation fail"
    )
    set_tests_properties(${GC_ALGORITHM}_overflowing_ptr_table PROPERTIES
        PASS_REGULAR_EXPRESSION "Invalid pointer table"
    )

    foreach(PTR_TABLE_CASE zero-array bad-offset bad-stride large-payload)
        string(REPLACE "-" "_" PTR_TABLE_TEST_SUFFIX ${PTR_TABLE_CASE})
        set(PTR_TABLE_TEST_NAME ${GC_ALGORITHM}_ptr_table_${PTR_TABLE_TEST_SUFFIX})
        add_test(NAME ${PTR_TABLE_TEST_NAME}
                 COMMAND ${GC_ALGORITHM}_overflowing_ptr_table ${PTR_TABLE_CASE})
        set_tests_properties(${PTR_TABLE_TEST_NAME} PROPERTIES
            PASS_REGULAR_EXPRESSION "Invalid pointer table"
        )
    endforeach()
endforeach()

foreach(GC_ALGORITHM ${GC_ALGORITHMS})
    set(EXECUTABLE_NAME ${GC_ALGORITHM}_cpp_exception_boundary)
    add_executable(${EXECUTABLE_NAME} cpp_exception_boundary.cpp)
    target_link_libraries(${EXECUTABLE_NAME} PRIVATE gc_${GC_ALGORITHM})
    gc_apply_build_options(${EXECUTABLE_NAME})
    add_test(NAME ${EXECUTABLE_NAME} COMMAND ${EXECUTABLE_NAME} root)
    set_tests_properties(${EXECUTABLE_NAME} PROPERTIES
        PASS_REGULAR_EXPRESSION "Allocation failure"
    )

    set(PTR_TABLE_EXCEPTION_TEST ${GC_ALGORITHM}_ptr_table_exception_boundary)
    add_test(NAME ${PTR_TABLE_EXCEPTION_TEST} COMMAND ${EXECUTABLE_NAME} pointer-table)
    set_tests_properties(${PTR_TABLE_EXCEPTION_TEST} PROPERTIES
        PASS_REGULAR_EXPRESSION "Allocation failure"
    )

    set(LIFECYCLE_EXECUTABLE ${GC_ALGORITHM}_invalid_lifecycle)
    add_executable(${LIFECYCLE_EXECUTABLE} invalid_lifecycle.c)
    target_link_libraries(${LIFECYCLE_EXECUTABLE} PRIVATE gc_${GC_ALGORITHM})
    gc_apply_build_options(${LIFECYCLE_EXECUTABLE})

    foreach(LIFECYCLE_CASE malloc collect after-cleanup)
        string(REPLACE "-" "_" LIFECYCLE_SUFFIX ${LIFECYCLE_CASE})
        set(LIFECYCLE_TEST ${GC_ALGORITHM}_invalid_lifecycle_${LIFECYCLE_SUFFIX})
        add_test(NAME ${LIFECYCLE_TEST} COMMAND ${LIFECYCLE_EXECUTABLE} ${LIFECYCLE_CASE})
        set_tests_properties(${LIFECYCLE_TEST} PROPERTIES
            PASS_REGULAR_EXPRESSION "GC is not initialized"
        )
    endforeach()
endforeach()
