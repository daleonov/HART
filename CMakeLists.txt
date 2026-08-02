cmake_minimum_required(VERSION 3.10)  # Can build with older cmake versions, but pre-3.10 requirement triggers warning on modern systems
project(HART VERSION 0.2.0 LANGUAGES CXX)
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

set(HART_TEST_SOURCES
        tests/generate_data.cpp
        tests/test_assertion_macros.cpp
        tests/test_audio_buffer.cpp
        tests/test_contracts.cpp
        tests/test_dsp.cpp
        tests/test_dsp_chains.cpp
        tests/test_dsp_sequence.cpp
        tests/test_envelope.cpp
        tests/test_host.cpp
        tests/test_impulse_response.cpp
        tests/test_main.cpp
        tests/test_matchers.cpp
        tests/test_metrics.cpp
        tests/test_reducers.cpp
        tests/test_signal.cpp
        tests/test_sine_sweep.cpp
        tests/test_time_it.cpp
    )

option(HART_BUILD_TESTS "Build HART tests" ON)

if(HART_BUILD_TESTS)
    add_executable(HART_Tests ${HART_TEST_SOURCES})
    target_link_libraries(HART_Tests PRIVATE HART)

    enable_testing()
    add_test(
        NAME HART_Tests
        COMMAND HART_Tests "--data-root-path" "${CMAKE_SOURCE_DIR}/tests/data/"
    )
endif()  # HART_BUILD_TESTS

option(HART_CHECK_CPP_STANDARDS_COMPATIBILITY "Build a bunch of dummy targets to check compatibility with major C++ standards" OFF)

if(HART_CHECK_CPP_STANDARDS_COMPATIBILITY)
    foreach(STD 11 14 17 20)
        add_executable(HART_CompatibilityCheckForCpp${STD} ${HART_TEST_SOURCES})
        target_link_libraries(HART_CompatibilityCheckForCpp${STD} PRIVATE HART)
        set_target_properties(HART_CompatibilityCheckForCpp${STD} PROPERTIES CXX_STANDARD ${STD})
        target_compile_features(HART_CompatibilityCheckForCpp${STD} PRIVATE cxx_std_${STD})
        target_compile_options(HART_CompatibilityCheckForCpp${STD} PRIVATE
            $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-O0>
            $<$<CXX_COMPILER_ID:MSVC>:/Od>
        )
    endforeach()
endif()
