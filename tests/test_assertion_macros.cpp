#include <string>
#include <vector>
#include <chrono>

#include "hart.hpp"

HART_TEST ("Assertion Macros - Expect Full")
{
    constexpr int a = 4;
    constexpr int b = 5;

    const std::string alpha ("alpha");
    const std::string beta ("beta");

    const std::vector<int> v1 = {1, 2};
    const std::vector<int> v2 = {1, 3};

    const std::chrono::milliseconds t1 (5);
    const std::chrono::milliseconds t2 (10);

    HART_EXPECT_TRUE (a < b);
    HART_EXPECT_FALSE (a > b);
    HART_EXPECT_EQUAL (a + 1, b);
    HART_EXPECT_NOT_EQUAL (a, b);
    HART_EXPECT_LESS_THAN (a, b);
    HART_EXPECT_LESS_OR_EQUAL (a, b);
    HART_EXPECT_GREATER_THAN (b, a);
    HART_EXPECT_GREATER_OR_EQUAL (b, a);
    HART_EXPECT_IN_RANGE (5, 1, 10);

    HART_EXPECT_LESS_THAN (alpha, beta);
    HART_EXPECT_GREATER_OR_EQUAL (beta, beta);
    HART_EXPECT_NOT_EQUAL (alpha, beta);

    HART_EXPECT_LESS_THAN (v1, v2);
    HART_EXPECT_GREATER_OR_EQUAL (v2, v2);
    HART_EXPECT_NOT_EQUAL (v1, v2);

    HART_EXPECT_LESS_THAN (t1, t2);
    HART_EXPECT_GREATER_OR_EQUAL (t2, t2);
    HART_EXPECT_NOT_EQUAL (t1, t2);

    HART_EXPECT_FLOAT_EQUAL (1.0, 1.0, 0.001);
    HART_EXPECT_FLOAT_NOT_EQUAL (1.0, 1.01, 0.001);
}

HART_TEST ("Assertion Macros - Expect Short")
{
    constexpr int a = 4;
    constexpr int b = 5;

    const std::string alpha ("alpha");
    const std::string beta ("beta");

    const std::vector<int> v1 = {1, 2};
    const std::vector<int> v2 = {1, 3};

    const std::chrono::milliseconds t1 (5);
    const std::chrono::milliseconds t2 (10);

    HART_EXPECT_TRUE (a < b);
    HART_EXPECT_FALSE (a > b);
    HART_EXPECT_EQ (a + 1, b);
    HART_EXPECT_NE (a, b);
    HART_EXPECT_LT (a, b);
    HART_EXPECT_LE (a, b);
    HART_EXPECT_GT (b, a);
    HART_EXPECT_GE (b, a);
    HART_EXPECT_IN_RANGE (5, 1, 10);

    HART_EXPECT_LT (alpha, beta);
    HART_EXPECT_GE (beta, beta);
    HART_EXPECT_NE (alpha, beta);

    HART_EXPECT_LT (v1, v2);
    HART_EXPECT_GE (v2, v2);
    HART_EXPECT_NE (v1, v2);

    HART_EXPECT_LT (t1, t2);
    HART_EXPECT_GE (t2, t2);
    HART_EXPECT_NE (t1, t2);

    HART_EXPECT_FLOAT_EQ (1.0, 1.0, 0.001);
    HART_EXPECT_FLOAT_NE (1.0, 1.01, 0.001);
}

HART_TEST ("Assertion Macros - Assert Full")
{
    constexpr int a = 4;
    constexpr int b = 5;

    const std::string alpha ("alpha");
    const std::string beta ("beta");

    const std::vector<int> v1 = {1, 2};
    const std::vector<int> v2 = {1, 3};

    const std::chrono::milliseconds t1 (5);
    const std::chrono::milliseconds t2 (10);

    HART_ASSERT_TRUE (a < b);
    HART_ASSERT_FALSE (a > b);
    HART_ASSERT_EQUAL (a + 1, b);
    HART_ASSERT_NOT_EQUAL (a, b);
    HART_ASSERT_LESS_THAN (a, b);
    HART_ASSERT_LESS_OR_EQUAL (a, b);
    HART_ASSERT_GREATER_THAN (b, a);
    HART_ASSERT_GREATER_OR_EQUAL (b, a);
    HART_ASSERT_IN_RANGE (5, 1, 10);

    HART_ASSERT_LESS_THAN (alpha, beta);
    HART_ASSERT_GREATER_OR_EQUAL (beta, beta);
    HART_ASSERT_NOT_EQUAL (alpha, beta);

    HART_ASSERT_LESS_THAN (v1, v2);
    HART_ASSERT_GREATER_OR_EQUAL (v2, v2);
    HART_ASSERT_NOT_EQUAL (v1, v2);

    HART_ASSERT_LESS_THAN (t1, t2);
    HART_ASSERT_GREATER_OR_EQUAL (t2, t2);
    HART_ASSERT_NOT_EQUAL (t1, t2);

    HART_ASSERT_FLOAT_EQUAL (1.0, 1.0, 0.001);
    HART_ASSERT_FLOAT_NOT_EQUAL (1.0, 1.01, 0.001);
}

HART_TEST ("Assertion Macros - Assert Short")
{
    constexpr int a = 4;
    constexpr int b = 5;

    const std::string alpha ("alpha");
    const std::string beta ("beta");

    const std::vector<int> v1 = {1, 2};
    const std::vector<int> v2 = {1, 3};

    const std::chrono::milliseconds t1 (5);
    const std::chrono::milliseconds t2 (10);

    HART_ASSERT_TRUE (a < b);
    HART_ASSERT_FALSE (a > b);
    HART_ASSERT_EQ (a + 1, b);
    HART_ASSERT_NE (a, b);
    HART_ASSERT_LT (a, b);
    HART_ASSERT_LE (a, b);
    HART_ASSERT_GT (b, a);
    HART_ASSERT_GE (b, a);
    HART_ASSERT_IN_RANGE (5, 1, 10);

    HART_ASSERT_LT (alpha, beta);
    HART_ASSERT_GE (beta, beta);
    HART_ASSERT_NE (alpha, beta);

    HART_ASSERT_LT (v1, v2);
    HART_ASSERT_GE (v2, v2);
    HART_ASSERT_NE (v1, v2);

    HART_ASSERT_LT (t1, t2);
    HART_ASSERT_GE (t2, t2);
    HART_ASSERT_NE (t1, t2);

    HART_ASSERT_FLOAT_EQ (1.0, 1.0, 0.001);
    HART_ASSERT_FLOAT_NE (1.0, 1.01, 0.001);
}

HART_TEST ("Assertion Macros - Boundaries")
{
    HART_EXPECT_GE (5, 5);
    HART_EXPECT_LE (5, 5);

    HART_EXPECT_IN_RANGE (1, 1, 10);
    HART_EXPECT_IN_RANGE (10, 1, 10);

    HART_EXPECT_FLOAT_EQ (1.0, 1.001, 0.001);
}

HART_TEST ("Assertion Macros - Float Tolerance")
{
    const double ref = 1.0;

    HART_EXPECT_FLOAT_EQ (ref, 1.0, 0.001);  // exact
    HART_EXPECT_FLOAT_EQ (ref, 1.0005, 0.001);  // inside tolerance
    HART_EXPECT_FLOAT_NE (ref, 1.01, 0.001);  // outside tolerance
}

HART_TEST ("Assertion Macros - Single Evaluation")
{
    int x = 0;
    HART_EXPECT_EQ (++x, 1);
    HART_EXPECT_EQ (x, 1);

    int lhs = 0;
    int rhs = 0;
    HART_EXPECT_EQ (++lhs, ++rhs);
    HART_EXPECT_EQ (lhs, 1);
    HART_EXPECT_EQ (rhs, 1);

    std::string s("a");
    HART_EXPECT_EQ (s += "b", std::string ("ab"));
    HART_EXPECT_EQ (s, std::string ("ab"));
}

HART_TEST ("Assertion Macros - Labels")
{
    HART_EXPECT_EQUAL (1 + 2, 3) << "Blah blah blah";
    HART_EXPECT_EQUAL (1 + 2, 3) << "More " << 123 << " complex " << 3.14f * 2 << " label";
    HART_ASSERT_EQUAL (1 + 2, 3) << "Same with assert level statements";
}
