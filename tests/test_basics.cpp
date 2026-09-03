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
    HART_EXPECT_EQ (2 + 2, 4);
}

HART_PARAMETRIC_TEST_WITH_TAGS ("Basics - Parametric test case with tags", "[some-tag][some-other-tag]")
{
    HART_EXPECT_EQ (2 + 2, 4);
}
