#include <iostream>
#include "hart.hpp"

HART_TEST ("Example1")
{
}

HART_TEST_WITH_TAGS ("Example2", "[tag1][tag2]")
{
    HART_ASSERT_TRUE (5 * 5 == 25);
    HART_EXPECT_TRUE (2 == 3);
    HART_EXPECT_TRUE (5 == 3);
    HART_EXPECT_TRUE (2 == 2);
    HART_EXPECT_TRUE (2 == 6);
    HART_ASSERT_TRUE (5 * 5 == 24);
    HART_EXPECT_TRUE (2 == 6);  // Unreachable
}

HART_TEST ("Example3")
{
    HART_ASSERT_TRUE (5 * 5 == 25);
    HART_EXPECT_TRUE (2 * 2 == 4);
}

int main (int argc, char** argv)
{
    return HART_RUN_ALL_TESTS (argc, argv);
}
