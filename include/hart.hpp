#pragma once

#if defined (HART_IMPLEMENTATION)
#define DR_WAV_IMPLEMENTATION  // Wav single header library's implementation
#endif

#include <stdexcept>

#include "hart_audio_buffer.hpp"
#include "dsp/hart_dsp_all.hpp"
#include "hart_cliconfig.hpp"
#include "envelopes/hart_envelopes_all.hpp"
#include "hart_exceptions.hpp"
#include "hart_expectation_failure_messages.hpp"
#include "matchers/hart_matchers_all.hpp"
#include "hart_process_audio.hpp"
#include "signals/hart_signals_all.hpp"
#include "hart_str.hpp"
#include "hart_test_registry.hpp"
#include "hart_units.hpp"
#include "hart_wavwriter.hpp"

namespace hart
{

/// @brief Fails a test case unconditionally with a text message
/// @param message Message to be displayed
/// @ingroup TestRunner
#define HART_FAIL_TEST_MSG(message) throw hart::TestAssertException (std::string ("HART_FAIL_TEST_MSG() triggered test fail at line ") + std::to_string (__LINE__) + " with message: \"" + message + '\"')

/// @brief Fails a test case unconditionally
/// @ingroup TestRunner
#define HART_FAIL_TEST() throw hart::TestAssertException (std::string ("HART_FAIL_TEST() triggered test fail at line ") + std::to_string (__LINE__))

/// @brief Use to check some condition inside a test case. Failing will abort the test runner.
/// @details Helpful for free-standing checks (before or after rendering audio)
/// @param condition A condition or boolean value to be checked. Will be triggered if it evaluates to `false`.
/// @ingroup TestRunner
#define HART_ASSERT_TRUE(condition) \
    if (!(condition)) throw hart::TestAssertException (std::string ("HART_ASSERT_TRUE() failed at line ") + std::to_string (__LINE__) + ": \"" #condition "\"");

/// @brief Use to check some condition inside a test case. Failing will not abort the test runner.
/// @details Helpful for free-standing checks (before or after rendering audio)
/// @param condition A condition or boolean value to be checked. Will be triggered if it evaluates to `false`.
/// @ingroup TestRunner
#define HART_EXPECT_TRUE(condition) \
    if (!(condition)) hart::ExpectationFailureMessages::get().emplace_back (std::string ("HART_EXPECT_TRUE() failed at line ") + std::to_string (__LINE__) + ": \"" #condition "\"");

#define HART_CONCAT_IMPL(x, y) x##y
#define HART_CONCAT(x, y) HART_CONCAT_IMPL(x, y)
#define HART_UNIQUE_ID(x) HART_CONCAT(x, __LINE__)

#define HART_ITEM_WITH_TAGS(name, tags, category) \
    static void HART_UNIQUE_ID(HART_RunTask)(); \
        namespace { \
            struct HART_UNIQUE_ID(HART_RegistrarType) { \
                HART_UNIQUE_ID(HART_RegistrarType)() { \
                    hart::TestRegistry::getInstance().add (name, tags, category, &HART_UNIQUE_ID (HART_RunTask)); \
                } \
            }; \
        } \
    static HART_UNIQUE_ID(HART_RegistrarType) HART_UNIQUE_ID(HART_registrar); \
    static void HART_UNIQUE_ID(HART_RunTask)()

/// @brief Declares a test case with tags
/// @warning Tags aren't supported yet
/// @param name Name for the test case
/// @param tags Tags like "[my-tag-1][my-tag-2]"
/// @ingroup TestRunner
#define HART_TEST_WITH_TAGS(name, tags) HART_ITEM_WITH_TAGS(name, tags, hart::TaskCategory::test)

/// @brief Declares a generator with tags
/// @details Pretty much the same as a usual test case, but will be called only if the `--run-generators` CLI flag is set
/// @warning Tags aren't supported yet
/// @param name Name for the generator
/// @param tags Tags like "[my-tag-1][my-tag-2]"
/// @ingroup TestRunner
#define HART_GENERATE_WITH_TAGS(name, tags) HART_ITEM_WITH_TAGS(name, tags, hart::TaskCategory::generate)

/// @brief Declares a test case
/// @param name Name for the test case
/// @ingroup TestRunner
#define HART_TEST(name) HART_TEST_WITH_TAGS(name, "")

/// @brief Declares a generator
/// @details Pretty much the same as a usual test case, but will be called only if the `--run-generators` CLI flag is set
/// @param name Name for generator
/// @ingroup TestRunner
#define HART_GENERATE(name) HART_GENERATE_WITH_TAGS(name, "")

#if HART_DO_NOT_THROW_EXCEPTIONS
/// @brief Put it at the beginning of your tese case if it requires a properly set data path
/// @details For example, when using relative paths to the wav files. The test will instantly fail is the path is not set.
/// @ingroup TestRunner
#define HART_REQUIRES_DATA_PATH_ARG if (hart::CLIConfig::getInstance().getDataRootPath().empty()) { hart::ExpectationFailureMessages::get().emplace_back ("This test requires a data path set by the --data-root-path CLI argument, but it's empty"); return; }
#else
/// @brief Put it at the beginning of your tese case if it requires a properly set data path
/// @details For example, when using relative paths to the wav files. The test will instantly fail is the path is not set.
/// @ingroup TestRunner
#define HART_REQUIRES_DATA_PATH_ARG if (hart::CLIConfig::getInstance().getDataRootPath().empty()) { throw hart::ConfigurationError ("This test requires a data path set by the --data-root-path CLI argument, but it's empty"); }
#endif  // HART_DO_NOT_THROW_EXCEPTIONS

/// @brief Runs all tests or generators
/// @details Place this macro in your `main()` function
/// @ingroup TestRunner
#define HART_RUN_ALL_TESTS(argc, argv) \
    do \
    { \
        hart::CLIConfig::getInstance().initCommandLineArgs(); \
        CLI11_PARSE (hart::CLIConfig::getInstance().getCLIApp(), argc, argv); \
        return hart::TestRegistry::getInstance().runAll(); \
    } \
    while (false);

/// @brief Put it before you test cases to use hart classes without hart:: namespace prefix and explicit `float` template value
#define HART_DECLARE_ALIASES_FOR_FLOAT  using namespace hart::aliases_float

/// @brief Put it before you test cases to use hart classes without hart:: namespace prefix and explicit `double` template value
#define HART_DECLARE_ALIASES_FOR_DOUBLE using namespace hart::aliases_double

} // namespace hart
