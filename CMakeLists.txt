cmake_minimum_required(VERSION 3.19)
project(bbb-dmx VERSION 0.7.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(bbb_dmx INTERFACE)
add_library(bbb::dmx ALIAS bbb_dmx)
target_include_directories(bbb_dmx INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
target_compile_features(bbb_dmx INTERFACE cxx_std_17)

option(BBB_DMX_BUILD_TESTS "Build bbb-dmx tests" ON)
if(BBB_DMX_BUILD_TESTS)
    enable_testing()
    add_executable(bbb_dmx_core_tests
        tests/bbb_dmx_core_tests.cpp
    )
    target_link_libraries(bbb_dmx_core_tests PRIVATE bbb::dmx)
    add_test(NAME bbb_dmx_core_tests COMMAND bbb_dmx_core_tests)
endif()
