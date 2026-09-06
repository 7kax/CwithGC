if(NOT DEFINED PROGRAM OR PROGRAM STREQUAL "")
    message(FATAL_ERROR "PROGRAM must name the executable under test")
endif()

if(NOT EXISTS "${PROGRAM}")
    message(FATAL_ERROR "Test executable does not exist: ${PROGRAM}")
endif()

if(NOT DEFINED EXPECTED_STDERR_REGEX OR EXPECTED_STDERR_REGEX STREQUAL "")
    message(FATAL_ERROR "EXPECTED_STDERR_REGEX must not be empty")
endif()

set(program_arguments)
set(collect_arguments OFF)
math(EXPR last_argument "${CMAKE_ARGC} - 1")
foreach(argument_index RANGE 0 ${last_argument})
    if(collect_arguments)
        list(APPEND program_arguments "${CMAKE_ARGV${argument_index}}")
    elseif(CMAKE_ARGV${argument_index} STREQUAL "--")
        set(collect_arguments ON)
    endif()
endforeach()

execute_process(
    COMMAND "${PROGRAM}" ${program_arguments}
    RESULT_VARIABLE program_result
    OUTPUT_VARIABLE program_stdout
    ERROR_VARIABLE program_stderr
    TIMEOUT 10
)

if(program_result STREQUAL "0")
    message(FATAL_ERROR
            "Expected ${PROGRAM} to fail, but it exited successfully.\n"
            "stdout:\n${program_stdout}\n"
            "stderr:\n${program_stderr}")
endif()

if(NOT program_result MATCHES "^[1-9][0-9]*$" AND
   NOT program_result STREQUAL "Subprocess aborted")
    message(FATAL_ERROR
            "${PROGRAM} did not produce an accepted failure result: ${program_result}\n"
            "stdout:\n${program_stdout}\n"
            "stderr:\n${program_stderr}")
endif()

if(NOT program_stderr MATCHES "${EXPECTED_STDERR_REGEX}")
    message(FATAL_ERROR
            "${PROGRAM} stderr did not match '${EXPECTED_STDERR_REGEX}'.\n"
            "stdout:\n${program_stdout}\n"
            "stderr:\n${program_stderr}")
endif()
