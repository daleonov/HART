cmake_minimum_required(VERSION 3.10)  # Can build with older cmake versions, but pre-3.10 requirement triggers warning on modern systems
project(HART VERSION 0.1.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra -Wpedantic -Wunused)
endif()

add_library(HART INTERFACE)
add_library(HART::HART ALIAS HART)

target_include_directories(HART INTERFACE
    include/
)

option(HART_BUILD_TESTS "Build HART tests" ON)

if(HART_BUILD_TESTS)
    add_executable(HART_Tests
        tests/generate_data.cpp
        tests/test_dsp.cpp
        tests/test_dsp_chains.cpp
        tests/test_envelope.cpp
        tests/test_host.cpp
        tests/test_main.cpp
        tests/test_sine_sweep.cpp
    )
    target_link_libraries(HART_Tests PRIVATE HART)

    enable_testing()
    add_test(
        NAME HART_Tests
        COMMAND HART_Tests "--data-root-path" "${CMAKE_SOURCE_DIR}/tests/data/"
    )
endif()  # HART_BUILD_TESTS
