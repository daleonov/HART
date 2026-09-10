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
        tests/test_basics.cpp
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
        tests/test_spectrum.cpp
        tests/test_time_it.cpp
        tests/test_utils.cpp
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

set(HART_CHECK_CPP_STANDARDS_COMPATIBILITY "" CACHE STRING "Build one dummy target to check compatibility with the requested C++ standard")
set_property(CACHE HART_CHECK_CPP_STANDARDS_COMPATIBILITY PROPERTY STRINGS "" OFF 11 14 17 20 23 26 23preview latest)

if(HART_CHECK_CPP_STANDARDS_COMPATIBILITY AND NOT HART_CHECK_CPP_STANDARDS_COMPATIBILITY STREQUAL "OFF")
    set(HART_CPP_COMPATIBILITY_TARGET HART_CompatibilityCheckForCpp${HART_CHECK_CPP_STANDARDS_COMPATIBILITY})
    add_executable(${HART_CPP_COMPATIBILITY_TARGET} ${HART_TEST_SOURCES})
    target_link_libraries(${HART_CPP_COMPATIBILITY_TARGET} PRIVATE HART)
    target_compile_options(${HART_CPP_COMPATIBILITY_TARGET} PRIVATE
        $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-O0>
        $<$<CXX_COMPILER_ID:MSVC>:/Od>
    )

    if(HART_CHECK_CPP_STANDARDS_COMPATIBILITY MATCHES "^[0-9]+$")
        set_target_properties(${HART_CPP_COMPATIBILITY_TARGET} PROPERTIES
            CXX_STANDARD ${HART_CHECK_CPP_STANDARDS_COMPATIBILITY}
            CXX_STANDARD_REQUIRED ON
            CXX_EXTENSIONS OFF
        )
    elseif(MSVC AND HART_CHECK_CPP_STANDARDS_COMPATIBILITY STREQUAL "23preview")
        # This target version is passed as MSVC compiler flag, rather than CMake option
        target_compile_options(${HART_CPP_COMPATIBILITY_TARGET} PRIVATE /std:c++23preview)
    elseif(MSVC AND HART_CHECK_CPP_STANDARDS_COMPATIBILITY STREQUAL "latest")
        # This target version is passed as MSVC compiler flag, rather than CMake option
        target_compile_options(${HART_CPP_COMPATIBILITY_TARGET} PRIVATE /std:c++latest)
    endif()
endif()
