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
#include "hart_test_registry.hpp"
#include "hart_units.hpp"
#include "hart_wavwriter.hpp"

namespace hart
{

#define HART_FAIL_TEST_MSG(msg) throw hart::TestAssertException (std::string ("HART_FAIL_TEST_MSG() triggered test fail at line ") + std::to_string (__LINE__) + " with message: \"" + msg + '\"')
#define HART_FAIL_TEST() throw hart::TestAssertException (std::string ("HART_FAIL_TEST() triggered test fail at line ") + std::to_string (__LINE__))

#define HART_ASSERT_TRUE(cond) \
    if (!(cond)) throw hart::TestAssertException (std::string ("HART_ASSERT_TRUE() failed at line ") + std::to_string (__LINE__) + ": \"" #cond "\"");

#define HART_EXPECT_TRUE(cond) \
    if (!(cond)) hart::ExpectationFailureMessages::get().emplace_back (std::string ("HART_EXPECT_TRUE() failed at line ") + std::to_string (__LINE__) + ": \"" #cond "\"");

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
/// @prief Place this macro in your `main()` function
/// @ingroup TestRunner
#define HART_RUN_ALL_TESTS(argc, argv) \
    do \
    { \
        hart::CLIConfig::getInstance().initCommandLineArgs(); \
        CLI11_PARSE (hart::CLIConfig::getInstance().getCLIApp(), argc, argv); \
        return hart::TestRegistry::getInstance().runAll(); \
    } \
    while (false);

} // namespace hart
