cmake_minimum_required(VERSION 3.10)  # Can build with older cmake versions, but pre-3.10 requirement triggers warning on modern systems
project(HART VERSION 0.0.1 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra -Wpedantic -Wunused)
endif()

add_library(HART INTERFACE)

target_include_directories(HART INTERFACE
    include/
    dependencies/CLI11/
    dependencies/dr_libs/
)

add_executable(HART_Examples
    src/hart.cpp
    examples/run_tests.cpp
    examples/example_test_booster.cpp
)

target_link_libraries(HART_Examples PRIVATE HART)

target_include_directories(HART_Examples PRIVATE
    examples/
)

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
