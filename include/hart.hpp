#pragma once

#include "hart_test_registry.hpp"

namespace hart
{

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
