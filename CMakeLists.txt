cmake_minimum_required(VERSION 3.10)
project(CwithGC LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Enable testing.
enable_testing()

# Add the library directory.
add_subdirectory(src)

# Add the test directory.
add_subdirectory(test)

# Add the verification directory.
add_subdirectory(verify)
