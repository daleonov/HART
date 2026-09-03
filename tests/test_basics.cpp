#include <array>

#include "hart.hpp"

HART_TEST ("Basics - Simple one-shot test case")
{
    HART_EXPECT_EQ (2 + 2, 4);
}

HART_TEST_WITH_TAGS ("Basics - One-shot test case with tags", "[some-tag][some-other-tag]")
{
    HART_EXPECT_EQ (2 + 2, 4);
}

HART_PARAMETRIC_TEST ("Basics - Parametric test case")
{
    const int x = HART_GENERATE_VALUE (11, 22, 33, 44);
    HART_EXPECT_EQ (x * 2,  x + x);
}

HART_PARAMETRIC_TEST_WITH_TAGS ("Basics - Parametric test case with tags", "[some-tag][some-other-tag]")
{
    const int x = HART_GENERATE_VALUE (11, 22, 33, 44);
    HART_EXPECT_EQ (x * 2,  x + x);
}

HART_PARAMETRIC_TEST ("Basics - Parametric test case with multiple parametrics and captures")
{
    const int x = HART_GENERATE_VALUE (11, 22, 33, 44);
    const int y = HART_GENERATE_VALUE (555, 666, 777);
    HART_CAPTURE_VALUE (x);
    HART_CAPTURE_VALUE (y);

    HART_EXPECT_EQ (x + y,  y + x);
}

HART_TEST ("Basics - One-shot test with a value capture")
{
    for (size_t i = 0; i < 10; ++i)
    {
        HART_CAPTURE_VALUE (i);
        HART_EXPECT_LT (i, 10);
    }
}

HART_PARAMETRIC_TEST("Basics - Parametric test with values from iterators")
{
    constexpr std::array<int, 3> values {{ 11, 22, 33 }};
    const int value = HART_GENERATE_VALUE (values.begin(), values.end());

    HART_EXPECT_GT (value, 0);
}
