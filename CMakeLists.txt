cmake_minimum_required(VERSION 3.20)
project(CwithGC LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(GC_ENABLE_DEBUG_API "Build the collector inspection API" ON)
option(GC_ENABLE_STRICT_WARNINGS "Enable strict compiler warnings" OFF)
option(GC_ENABLE_SANITIZERS "Enable AddressSanitizer and UndefinedBehaviorSanitizer" OFF)

add_library(gc_build_options INTERFACE)

if(GC_ENABLE_STRICT_WARNINGS)
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang" AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(gc_build_options INTERFACE
            -Wall
            -Wextra
            -Wpedantic
            -Werror
            # Tracked in TODO.md until root lifetime no longer depends on caller frames.
            -Wno-error=frame-address
        )
    elseif(MSVC)
        target_compile_options(gc_build_options INTERFACE /W4 /WX)
    else()
        message(WARNING "Strict warning flags are not configured for this compiler")
    endif()
endif()

if(GC_ENABLE_SANITIZERS)
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang" AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(gc_build_options INTERFACE
            -fsanitize=address,undefined
            -fno-omit-frame-pointer
        )
        target_link_options(gc_build_options INTERFACE -fsanitize=address,undefined)
    else()
        message(FATAL_ERROR "GC_ENABLE_SANITIZERS requires GCC or Clang")
    endif()
endif()

function(gc_apply_build_options TARGET)
    target_link_libraries(${TARGET} PRIVATE gc_build_options)
endfunction()

file(GLOB_RECURSE GC_FORMAT_SOURCES CONFIGURE_DEPENDS
    "${PROJECT_SOURCE_DIR}/include/*.h"
    "${PROJECT_SOURCE_DIR}/src/*.cpp"
    "${PROJECT_SOURCE_DIR}/src/*.hpp"
    "${PROJECT_SOURCE_DIR}/test/*.c"
    "${PROJECT_SOURCE_DIR}/test/*.h"
    "${PROJECT_SOURCE_DIR}/test/*.cpp"
    "${PROJECT_SOURCE_DIR}/verify/*.c"
    "${PROJECT_SOURCE_DIR}/verify/*.cpp"
)

find_program(CLANG_FORMAT_EXECUTABLE NAMES clang-format clang-format-18)
if(CLANG_FORMAT_EXECUTABLE)
    add_custom_target(format
        COMMAND ${CLANG_FORMAT_EXECUTABLE} -i ${GC_FORMAT_SOURCES}
        COMMENT "Formatting C and C++ sources"
        VERBATIM
    )
    add_custom_target(format-check
        COMMAND ${CLANG_FORMAT_EXECUTABLE} --dry-run --Werror ${GC_FORMAT_SOURCES}
        COMMENT "Checking C and C++ formatting"
        VERBATIM
    )
else()
    message(STATUS "clang-format not found; format targets are unavailable")
endif()

find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_Interpreter_FOUND)
    add_custom_target(language-check
        COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/cmake/check_english.py
                ${PROJECT_SOURCE_DIR}
        COMMENT "Checking repository text for non-English documentation and comments"
        VERBATIM
    )
else()
    message(STATUS "Python 3 not found; language-check target is unavailable")
endif()

if(TARGET format-check AND TARGET language-check)
    add_custom_target(quality-check DEPENDS format-check language-check)
endif()

# Add the library directory.
add_subdirectory(src)

include(CTest)
if(BUILD_TESTING)
    if(NOT GC_ENABLE_DEBUG_API)
        message(FATAL_ERROR "BUILD_TESTING requires GC_ENABLE_DEBUG_API")
    endif()

    # Add the test and verification directories.
    add_subdirectory(test)
    add_subdirectory(verify)
endif()
