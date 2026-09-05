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
    set_target_properties(${C_SMOKE_TARGET} PROPERTIES C_EXTENSIONS OFF)
    add_test(NAME ${C_SMOKE_TARGET} COMMAND ${C_SMOKE_TARGET})

    set(CPP_SMOKE_TARGET ${GC_ALGORITHM}_cpp_api_smoke)
    add_executable(${CPP_SMOKE_TARGET} cpp_api_smoke.cpp)
    target_link_libraries(${CPP_SMOKE_TARGET} PRIVATE gc_${GC_ALGORITHM})
    set_target_properties(${CPP_SMOKE_TARGET} PROPERTIES CXX_EXTENSIONS OFF)
    add_test(NAME ${CPP_SMOKE_TARGET} COMMAND ${CPP_SMOKE_TARGET})

    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${C_SMOKE_TARGET} PRIVATE -Wall -Wextra -Wpedantic -Werror
                                                         -pedantic-errors)
    endif()
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${CPP_SMOKE_TARGET} PRIVATE -Wall -Wextra -Wpedantic -Werror
                                                           -pedantic-errors)
    endif()
endforeach()

# Create tests for each algorithm.
foreach(GC_ALGORITHM ${GC_ALGORITHMS})
    foreach(TEST_NAME ${TEST_NAMES})
        set(TEST_FILE ${CMAKE_CURRENT_SOURCE_DIR}/${GC_ALGORITHM}/${TEST_NAME}.c)
        if(EXISTS ${TEST_FILE})
            add_executable(${GC_ALGORITHM}_${TEST_NAME} ${GC_ALGORITHM}/${TEST_NAME}.c)
            target_link_libraries(${GC_ALGORITHM}_${TEST_NAME} PRIVATE gc_${GC_ALGORITHM})
            add_test(NAME ${GC_ALGORITHM}_${TEST_NAME} COMMAND ${GC_ALGORITHM}_${TEST_NAME})
        endif()
    endforeach()
endforeach()

# Shared allocation-contract tests run against every collector.
foreach(GC_ALGORITHM ${GC_ALGORITHMS})
    add_executable(${GC_ALGORITHM}_alignment alignment.c)
    target_link_libraries(${GC_ALGORITHM}_alignment PRIVATE gc_${GC_ALGORITHM})
    add_test(NAME ${GC_ALGORITHM}_alignment COMMAND ${GC_ALGORITHM}_alignment)

    add_executable(${GC_ALGORITHM}_lifecycle lifecycle.c)
    target_link_libraries(${GC_ALGORITHM}_lifecycle PRIVATE gc_${GC_ALGORITHM})
    add_test(NAME ${GC_ALGORITHM}_lifecycle COMMAND ${GC_ALGORITHM}_lifecycle)
endforeach()
