cmake_minimum_required(VERSION 3.14)
project(asun VERSION 1.0.0 LANGUAGES CXX)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(ASUN_BUILD_EXAMPLES "Build asun-cpp examples" ON)
option(ASUN_BUILD_TESTS "Build asun-cpp tests" ON)

# Release by default
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")

# Header-only library
add_library(asun INTERFACE)
add_library(asun::asun ALIAS asun)
target_include_directories(asun INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
target_compile_features(asun INTERFACE cxx_std_17)

if(ASUN_BUILD_EXAMPLES)
    add_executable(basic examples/basic.cpp)
    target_link_libraries(basic PRIVATE asun)

    add_executable(complex_example examples/complex.cpp)
    target_link_libraries(complex_example PRIVATE asun)

    add_executable(bench examples/bench.cpp)
    target_link_libraries(bench PRIVATE asun)

    add_executable(cross_compat examples/cross_compat.cpp)
    target_link_libraries(cross_compat PRIVATE asun)
endif()

if(ASUN_BUILD_TESTS)
    enable_testing()

    add_executable(asun_test tests/asun_test.cpp)
    target_link_libraries(asun_test PRIVATE asun)
    add_test(NAME asun_test COMMAND asun_test)

    add_executable(cross_compat_test tests/cross_compat_test.cpp)
    target_link_libraries(cross_compat_test PRIVATE asun)
    add_test(NAME cross_compat_test COMMAND cross_compat_test)
endif()

install(
    TARGETS asun
    EXPORT asunTargets
)

install(
    DIRECTORY include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(
    EXPORT asunTargets
    FILE asunTargets.cmake
    NAMESPACE asun::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/asun
)

configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/asunConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/asunConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/asun
)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/asunConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(
    FILES
        ${CMAKE_CURRENT_BINARY_DIR}/asunConfig.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/asunConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/asun
)
