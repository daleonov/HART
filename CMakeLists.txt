cmake_minimum_required(VERSION 3.23)
project(HART VERSION 0.0.1 LANGUAGES C CXX)

add_executable(${PROJECT_NAME}
    src/hart.cpp
    examples/run_tests.cpp
    examples/example_test_booster.cpp
)

target_include_directories(${PROJECT_NAME} PRIVATE
    include/
    examples/
    dependencies/CLI11/
    dependencies/dr_libs/
)
