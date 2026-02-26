cmake_minimum_required(VERSION 3.14)
project(ason VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Release by default
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")

# Header-only library
add_library(ason INTERFACE)
target_include_directories(ason INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)

# Examples
add_executable(basic examples/basic.cpp)
target_link_libraries(basic PRIVATE ason)

add_executable(complex_example examples/complex.cpp)
target_link_libraries(complex_example PRIVATE ason)

add_executable(bench examples/bench.cpp)
target_link_libraries(bench PRIVATE ason)

add_executable(cross_compat examples/cross_compat.cpp)
target_link_libraries(cross_compat PRIVATE ason)

# Tests
enable_testing()
add_executable(ason_test tests/ason_test.cpp)
target_link_libraries(ason_test PRIVATE ason)
add_test(NAME ason_test COMMAND ason_test)

add_executable(cross_compat_test tests/cross_compat_test.cpp)
target_link_libraries(cross_compat_test PRIVATE ason)
add_test(NAME cross_compat_test COMMAND cross_compat_test)
