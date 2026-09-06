# Test names.
set(TEST_NAMES
    basic
    func_call
    recursion
    linked_list
    graph
    full_heap
    assignment
)

# Garbage-collection algorithms.
set(GC_ALGORITHMS
    mark_sweep
    copying
    ref_count
)

# Compile and link the public API from strict C11 and C++20 callers.
foreach(GC_ALGORITHM ${GC_ALGORITHMS})
    set(C_SMOKE_TARGET ${GC_ALGORITHM}_c_api_smoke)
    add_executable(${C_SMOKE_TARGET} c_api_smoke.c)
    target_link_libraries(${C_SMOKE_TARGET} PRIVATE gc_${GC_ALGORITHM})
    gc_apply_build_options(${C_SMOKE_TARGET})
    set_target_properties(${C_SMOKE_TARGET} PROPERTIES C_EXTENSIONS OFF)
    add_test(NAME ${C_SMOKE_TARGET} COMMAND ${C_SMOKE_TARGET})

    set(CPP_SMOKE_TARGET ${GC_ALGORITHM}_cpp_api_smoke)
    add_executable(${CPP_SMOKE_TARGET} cpp_api_smoke.cpp)
    target_link_libraries(${CPP_SMOKE_TARGET} PRIVATE gc_${GC_ALGORITHM})
    gc_apply_build_options(${CPP_SMOKE_TARGET})
    set_target_properties(${CPP_SMOKE_TARGET} PROPERTIES CXX_EXTENSIONS OFF)
    add_test(NAME ${CPP_SMOKE_TARGET} COMMAND ${CPP_SMOKE_TARGET})

    set(DEBUG_C_SMOKE_TARGET ${GC_ALGORITHM}_debug_api_smoke)
    add_executable(${DEBUG_C_SMOKE_TARGET} debug_api_smoke.c)
    target_link_libraries(${DEBUG_C_SMOKE_TARGET} PRIVATE gc_${GC_ALGORITHM})
    gc_apply_build_options(${DEBUG_C_SMOKE_TARGET})
    target_compile_definitions(${DEBUG_C_SMOKE_TARGET} PRIVATE
        TEST_EXPECT_INSPECTION=$<BOOL:${GC_ENABLE_INSPECTION}>
    )
    set_target_properties(${DEBUG_C_SMOKE_TARGET} PROPERTIES C_EXTENSIONS OFF)
    add_test(NAME ${DEBUG_C_SMOKE_TARGET} COMMAND ${DEBUG_C_SMOKE_TARGET})

    set(DEBUG_CPP_SMOKE_TARGET ${GC_ALGORITHM}_debug_api_cpp_smoke)
    add_executable(${DEBUG_CPP_SMOKE_TARGET} debug_api_cpp_smoke.cpp)
    target_link_libraries(${DEBUG_CPP_SMOKE_TARGET} PRIVATE gc_${GC_ALGORITHM})
    gc_apply_build_options(${DEBUG_CPP_SMOKE_TARGET})
    set_target_properties(${DEBUG_CPP_SMOKE_TARGET} PROPERTIES CXX_EXTENSIONS OFF)
    add_test(NAME ${DEBUG_CPP_SMOKE_TARGET} COMMAND ${DEBUG_CPP_SMOKE_TARGET})

    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${C_SMOKE_TARGET} PRIVATE -Wall -Wextra -Wpedantic -Werror
                                                         -pedantic-errors)
        target_compile_options(${DEBUG_C_SMOKE_TARGET} PRIVATE -Wall -Wextra -Wpedantic -Werror
                                                                -pedantic-errors)
    endif()
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${CPP_SMOKE_TARGET} PRIVATE -Wall -Wextra -Wpedantic -Werror
                                                           -pedantic-errors)
        target_compile_options(${DEBUG_CPP_SMOKE_TARGET} PRIVATE -Wall -Wextra -Wpedantic -Werror
                                                                  -pedantic-errors)
    endif()
endforeach()

if(GC_ENABLE_INSPECTION)
    # Create tests for each algorithm.
    foreach(GC_ALGORITHM ${GC_ALGORITHMS})
        foreach(TEST_NAME ${TEST_NAMES})
            set(TEST_FILE ${CMAKE_CURRENT_SOURCE_DIR}/${GC_ALGORITHM}/${TEST_NAME}.c)
            if(EXISTS ${TEST_FILE})
                add_executable(${GC_ALGORITHM}_${TEST_NAME} ${GC_ALGORITHM}/${TEST_NAME}.c)
                target_link_libraries(${GC_ALGORITHM}_${TEST_NAME} PRIVATE gc_${GC_ALGORITHM})
                gc_apply_build_options(${GC_ALGORITHM}_${TEST_NAME})
                add_test(NAME ${GC_ALGORITHM}_${TEST_NAME} COMMAND ${GC_ALGORITHM}_${TEST_NAME})
            endif()
        endforeach()
    endforeach()

    # Reference counting intentionally retains unreachable cycles until cleanup.
    add_executable(ref_count_cycle ref_count/cycle.c)
    target_link_libraries(ref_count_cycle PRIVATE gc_ref_count)
    gc_apply_build_options(ref_count_cycle)
    add_test(NAME ref_count_cycle COMMAND ref_count_cycle)

    # Shared allocation-contract tests run against every collector.
    foreach(GC_ALGORITHM ${GC_ALGORITHMS})
        add_executable(${GC_ALGORITHM}_alignment alignment.c)
        target_link_libraries(${GC_ALGORITHM}_alignment PRIVATE gc_${GC_ALGORITHM})
        gc_apply_build_options(${GC_ALGORITHM}_alignment)
        add_test(NAME ${GC_ALGORITHM}_alignment COMMAND ${GC_ALGORITHM}_alignment)

        add_executable(${GC_ALGORITHM}_lifecycle lifecycle.c)
        target_link_libraries(${GC_ALGORITHM}_lifecycle PRIVATE gc_${GC_ALGORITHM})
        gc_apply_build_options(${GC_ALGORITHM}_lifecycle)
        add_test(NAME ${GC_ALGORITHM}_lifecycle COMMAND ${GC_ALGORITHM}_lifecycle)

        add_executable(${GC_ALGORITHM}_scope_lifetime scope_lifetime.c)
        target_link_libraries(${GC_ALGORITHM}_scope_lifetime PRIVATE gc_${GC_ALGORITHM})
        gc_apply_build_options(${GC_ALGORITHM}_scope_lifetime)
        if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
            target_compile_options(${GC_ALGORITHM}_scope_lifetime PRIVATE -O2)
        endif()
        add_test(NAME ${GC_ALGORITHM}_scope_lifetime COMMAND ${GC_ALGORITHM}_scope_lifetime)
    endforeach()
endif()
