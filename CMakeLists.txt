cmake_minimum_required(VERSION 3.14)
project(ason VERSION 1.0.0 LANGUAGES CXX)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(ASON_BUILD_EXAMPLES "Build ason-cpp examples" ON)
option(ASON_BUILD_TESTS "Build ason-cpp tests" ON)

# Release by default
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")

# Header-only library
add_library(ason INTERFACE)
add_library(ason::ason ALIAS ason)
target_include_directories(ason INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
target_compile_features(ason INTERFACE cxx_std_17)

if(ASON_BUILD_EXAMPLES)
    add_executable(basic examples/basic.cpp)
    target_link_libraries(basic PRIVATE ason)

    add_executable(complex_example examples/complex.cpp)
    target_link_libraries(complex_example PRIVATE ason)

    add_executable(bench examples/bench.cpp)
    target_link_libraries(bench PRIVATE ason)

    add_executable(cross_compat examples/cross_compat.cpp)
    target_link_libraries(cross_compat PRIVATE ason)
endif()

if(ASON_BUILD_TESTS)
    enable_testing()

    add_executable(ason_test tests/ason_test.cpp)
    target_link_libraries(ason_test PRIVATE ason)
    add_test(NAME ason_test COMMAND ason_test)

    add_executable(cross_compat_test tests/cross_compat_test.cpp)
    target_link_libraries(cross_compat_test PRIVATE ason)
    add_test(NAME cross_compat_test COMMAND cross_compat_test)
endif()

install(
    TARGETS ason
    EXPORT asonTargets
)

install(
    DIRECTORY include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(
    EXPORT asonTargets
    FILE asonTargets.cmake
    NAMESPACE ason::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ason
)

configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/asonConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/asonConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ason
)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/asonConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(
    FILES
        ${CMAKE_CURRENT_BINARY_DIR}/asonConfig.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/asonConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ason
)
