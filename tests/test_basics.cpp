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
