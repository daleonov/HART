#pragma once

#include <exception>

#include "hart_test_registry.hpp"

namespace hart
{
#define HART_THROW(msg) throw std::runtime_error (msg)
#define HART_FAIL_TEST_MSG(msg) HART_THROW (msg)
#define HART_FAIL_TEST HART_THROW ("HART_FAIL_TEST triggered test fail")

#define HART_ASSERT_TRUE(cond) \
    if (!(cond)) throw std::runtime_error ("HART_ASSERT_TRUE() failed: \"" #cond "\"");

#define HART_EXPECT_TRUE(cond) \
    if (!(cond)) hart::expectationFailureMessages.emplace_back ("HART_EXPECT_TRUE() failed: \"" #cond "\"");

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

#define HART_RUN_ALL_TESTS() hart::TestRegistry::getInstance().runAll()

} // namespace hart
