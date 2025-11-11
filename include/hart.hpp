#pragma once

#if defined (HART_IMPLEMENTATION)
#define DR_WAV_IMPLEMENTATION  // Wav single header library's implementation
#endif

#include <stdexcept>

#include "hart_audio_buffer.hpp"
#include "dsp/hart_dsp_all.hpp"
#include "hart_exceptions.hpp"
#include "hart_expectation_failure_messages.hpp"
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

#define HART_TEST_WITH_TAGS(name, tags) \
    static void HART_UNIQUE_ID(func_)(); \
        namespace { \
            struct HART_UNIQUE_ID(registrar_) { \
                HART_UNIQUE_ID(registrar_)() { \
                    hart::TestRegistry::getInstance().add (name, tags, &HART_UNIQUE_ID(func_)); \
                } \
            }; \
        } \
        static HART_UNIQUE_ID(registrar_) HART_UNIQUE_ID(registrar_instance_); \
        static void HART_UNIQUE_ID(func_)()

#define HART_TEST(name) HART_TEST_WITH_TAGS(name, "")

#define HART_RUN_ALL_TESTS(argc, argv) hart::TestRegistry::getInstance().runAll (argc, argv)

} // namespace hart
