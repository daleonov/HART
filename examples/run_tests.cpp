#include <iostream>
#include "hart.hpp"

HART_TEST ("Example1")
{
}

HART_TEST_WITH_TAGS ("Example2", "[tag1][tag2]")
{
}

int main()
{
    return HART_RUN_ALL_TESTS();
}
