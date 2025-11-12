cmake_minimum_required(VERSION 3.23)
project(HART VERSION 0.0.1 LANGUAGES C CXX)

add_executable(HART_Examples
    src/hart.cpp
    examples/run_tests.cpp
    examples/example_test_booster.cpp
)

target_include_directories(HART_Examples PRIVATE
    include/
    examples/
    dependencies/CLI11/
    dependencies/dr_libs/
)

add_executable(HART_Tests
    tests/test_dsp.cpp
    tests/test_dsp_chains.cpp
    tests/test_envelope.cpp
    tests/test_main.cpp
)

target_include_directories(HART_Tests PRIVATE
    include/
    examples/
    dependencies/CLI11/
    dependencies/dr_libs/
)
