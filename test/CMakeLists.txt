# Garbage-collection algorithms.
set(GC_ALGORITHMS
    mark_sweep
    copying
    ref_count
)

function(add_collector_test GC_ALGORITHM TEST_TARGET SOURCE_FILE TEST_LABELS)
    add_executable(${TEST_TARGET} ${SOURCE_FILE})
    target_link_libraries(${TEST_TARGET} PRIVATE gc_${GC_ALGORITHM})
    gc_apply_build_options(${TEST_TARGET})
    add_test(NAME ${TEST_TARGET} COMMAND ${TEST_TARGET})
    set_tests_properties(${TEST_TARGET} PROPERTIES LABELS "${TEST_LABELS}")
endfunction()

function(add_collector_matrix_test TEST_NAME SOURCE_FILE TEST_LABELS)
    foreach(GC_ALGORITHM IN LISTS GC_ALGORITHMS)
        set(TEST_TARGET ${GC_ALGORITHM}_${TEST_NAME})
        add_collector_test(${GC_ALGORITHM} ${TEST_TARGET} ${SOURCE_FILE} "${TEST_LABELS}")
    endforeach()
endfunction()

function(add_collector_abi_smoke TEST_NAME SOURCE_FILE)
    foreach(GC_ALGORITHM IN LISTS GC_ALGORITHMS)
        set(TEST_TARGET ${GC_ALGORITHM}_${TEST_NAME})
        add_collector_test(${GC_ALGORITHM} ${TEST_TARGET} ${SOURCE_FILE} abi)
        if(SOURCE_FILE MATCHES "\\.c$")
            set_target_properties(${TEST_TARGET} PROPERTIES C_EXTENSIONS OFF)
        elseif(SOURCE_FILE MATCHES "\\.cpp$")
            set_target_properties(${TEST_TARGET} PROPERTIES CXX_EXTENSIONS OFF)
        endif()
    endforeach()
endfunction()

function(add_collector_debug_api_smoke TEST_NAME SOURCE_FILE)
    foreach(GC_ALGORITHM IN LISTS GC_ALGORITHMS)
        set(TEST_TARGET ${GC_ALGORITHM}_${TEST_NAME})
        add_collector_test(${GC_ALGORITHM} ${TEST_TARGET} ${SOURCE_FILE} "abi;inspection")
        if(SOURCE_FILE MATCHES "\\.c$")
            set_target_properties(${TEST_TARGET} PROPERTIES C_EXTENSIONS OFF)
        elseif(SOURCE_FILE MATCHES "\\.cpp$")
            set_target_properties(${TEST_TARGET} PROPERTIES CXX_EXTENSIONS OFF)
        endif()
        target_compile_definitions(${TEST_TARGET} PRIVATE
            TEST_EXPECT_INSPECTION=$<BOOL:${GC_ENABLE_INSPECTION}>
        )
    endforeach()
endfunction()

# Compile and link the runtime ABI from strict C11 and C++20 instrumentation-shaped inputs.
add_collector_abi_smoke(c_runtime_abi_smoke c_runtime_abi_smoke.c)
add_collector_abi_smoke(cpp_runtime_abi_smoke cpp_runtime_abi_smoke.cpp)
add_collector_debug_api_smoke(debug_api_smoke debug_api_smoke.c)
add_collector_debug_api_smoke(debug_api_cpp_smoke debug_api_cpp_smoke.cpp)

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    foreach(GC_ALGORITHM IN LISTS GC_ALGORITHMS)
        target_compile_options(${GC_ALGORITHM}_c_runtime_abi_smoke PRIVATE
                               -Wall -Wextra -Wpedantic -Werror -pedantic-errors)
        target_compile_options(${GC_ALGORITHM}_debug_api_smoke PRIVATE
                               -Wall -Wextra -Wpedantic -Werror -pedantic-errors)
    endforeach()
endif()
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    foreach(GC_ALGORITHM IN LISTS GC_ALGORITHMS)
        target_compile_options(${GC_ALGORITHM}_cpp_runtime_abi_smoke PRIVATE
                               -Wall -Wextra -Wpedantic -Werror -pedantic-errors)
        target_compile_options(${GC_ALGORITHM}_debug_api_cpp_smoke PRIVATE
                               -Wall -Wextra -Wpedantic -Werror -pedantic-errors)
    endforeach()
endif()

set(COLLECTOR_TEST_NAMES
    basic
    func_call
    recursion
    linked_list
    graph
    full_heap
    assignment
)

function(add_collector_specific_tests ALGORITHM)
    foreach(TEST_NAME IN LISTS COLLECTOR_TEST_NAMES)
        set(TEST_FILE ${CMAKE_CURRENT_SOURCE_DIR}/${ALGORITHM}/${TEST_NAME}.c)
        if(EXISTS ${TEST_FILE})
            add_collector_test(${ALGORITHM} ${ALGORITHM}_${TEST_NAME} ${TEST_FILE}
                               "conformance;inspection")
        endif()
    endforeach()
endfunction()

if(GC_ENABLE_INSPECTION)
    foreach(GC_ALGORITHM IN LISTS GC_ALGORITHMS)
        add_collector_specific_tests(${GC_ALGORITHM})
    endforeach()

    # Reference counting intentionally retains unreachable cycles until cleanup.
    add_collector_test(ref_count ref_count_cycle
                       ${CMAKE_CURRENT_SOURCE_DIR}/ref_count/cycle.c
                       "conformance;inspection")

    # Shared allocation-contract tests run against every collector.
    add_collector_matrix_test(alignment conformance/alignment.c "conformance;inspection")
    add_collector_matrix_test(lifecycle conformance/lifecycle.c "conformance;inspection")
    add_collector_matrix_test(scope_lifetime conformance/scope_lifetime.c
                              "conformance;inspection")
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        foreach(GC_ALGORITHM IN LISTS GC_ALGORITHMS)
            target_compile_options(${GC_ALGORITHM}_scope_lifetime PRIVATE -O2)
        endforeach()
    endif()

    # Reclamation checks use collector statistics from the optional inspection API.
    add_collector_matrix_test(unrooted_scalar_reclamation
                              conformance/unrooted_scalar_reclamation.c
                              "conformance;inspection")
    add_collector_matrix_test(unrooted_tree_reclamation
                              conformance/unrooted_tree_reclamation.c
                              "conformance;inspection")
    add_collector_matrix_test(array_object_reclamation
                              conformance/array_object_reclamation.c
                              "conformance;inspection")
endif()

# Root slots are part of the collector ABI and do not require inspection statistics.
add_collector_matrix_test(root_slot_initialization
                          conformance/root_slot_initialization.c
                          conformance)
