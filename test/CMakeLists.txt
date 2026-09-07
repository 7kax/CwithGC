# Garbage-collection algorithms.
set(GC_ALGORITHMS
    mark_sweep
    copying
    ref_count
)

function(add_collector_executable GC_ALGORITHM TEST_TARGET SOURCE_FILE)
    add_executable(${TEST_TARGET} ${SOURCE_FILE})
    target_link_libraries(${TEST_TARGET} PRIVATE gc_${GC_ALGORITHM})
    gc_apply_build_options(${TEST_TARGET})
endfunction()

function(add_collector_test GC_ALGORITHM TEST_TARGET SOURCE_FILE TEST_LABELS)
    add_collector_executable(${GC_ALGORITHM} ${TEST_TARGET} ${SOURCE_FILE})
    add_test(NAME ${TEST_TARGET} COMMAND ${TEST_TARGET})
    set_tests_properties(${TEST_TARGET} PROPERTIES LABELS "${TEST_LABELS}")
endfunction()

function(add_expected_failure_test TEST_NAME TEST_TARGET EXPECTED_STDERR TEST_LABELS)
    add_test(NAME ${TEST_NAME}
             COMMAND ${CMAKE_COMMAND}
                     "-DPROGRAM=$<TARGET_FILE:${TEST_TARGET}>"
                     "-DEXPECTED_STDERR_REGEX=${EXPECTED_STDERR}"
                     -P ${PROJECT_SOURCE_DIR}/cmake/expect_failure.cmake
                     -- ${ARGN})
    set_tests_properties(${TEST_NAME} PROPERTIES LABELS "${TEST_LABELS}" TIMEOUT 15)
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
endif()

# Shared allocation-contract tests run against every collector without relying
# on collector statistics or memory-layout snapshots.
add_collector_matrix_test(alignment conformance/alignment.c conformance)
add_collector_matrix_test(lifecycle conformance/lifecycle.c conformance)
add_collector_matrix_test(scope_lifetime conformance/scope_lifetime.c conformance)
add_collector_matrix_test(registered_graph conformance/registered_graph.c conformance)
add_collector_matrix_test(reclamation_pressure conformance/reclamation_pressure.c conformance)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    foreach(GC_ALGORITHM IN LISTS GC_ALGORITHMS)
        target_compile_options(${GC_ALGORITHM}_scope_lifetime PRIVATE -O2)
    endforeach()
endif()

if(GC_ENABLE_INSPECTION)

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

# Reachable runtime failures are executed as child processes so CTest can
# validate both their termination status and diagnostic output.
foreach(GC_ALGORITHM IN LISTS GC_ALGORITHMS)
    set(OVERSIZED_TARGET ${GC_ALGORITHM}_oversized_allocation)
    add_collector_executable(${GC_ALGORITHM} ${OVERSIZED_TARGET}
                             failure/oversized_allocation.c)
    add_expected_failure_test(${OVERSIZED_TARGET} ${OVERSIZED_TARGET}
                              "Allocation failure" failure)

    set(EXCEPTION_TARGET ${GC_ALGORITHM}_cpp_exception_boundary)
    add_collector_executable(${GC_ALGORITHM} ${EXCEPTION_TARGET}
                             failure/cpp_exception_boundary.cpp)
    add_expected_failure_test(${EXCEPTION_TARGET}_root ${EXCEPTION_TARGET}
                              "Allocation failure" failure root)
    add_expected_failure_test(${EXCEPTION_TARGET}_pointer_table ${EXCEPTION_TARGET}
                              "Allocation failure" failure pointer-table)
endforeach()

if(GC_ENABLE_INSPECTION)
    set(FULL_HEAP_TARGET mark_sweep_full_heap_failure)
    add_collector_executable(mark_sweep ${FULL_HEAP_TARGET}
                             failure/mark_sweep_full_heap.c)
    add_expected_failure_test(${FULL_HEAP_TARGET} ${FULL_HEAP_TARGET}
                              "Allocation failure" "failure;inspection")
endif()
