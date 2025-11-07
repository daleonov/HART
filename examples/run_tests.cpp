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
    HART_RUN_ALL_TESTS();
    std::cout << "HART Tests run" << std::endl;
    return 0;
}
