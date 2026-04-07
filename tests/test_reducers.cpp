#include <vector>

#include "hart.hpp"

HART_TEST ("Reducers - Argmin and argmax")
{
    using hart::argmax;
    using hart::argmin;

    const std::vector<double> values { 3.0, -1.0, 7.0, 2.0 };

    HART_EXPECT_TRUE (argmax() (values.begin(), values.end()) == 2);
    HART_EXPECT_TRUE (argmin() (values.begin(), values.end()) == 1);
}

HART_TEST ("Reducers - First and last")
{
    using hart::first;
    using hart::last;

    const std::vector<double> values { 3.0, -1.0, 7.0, 2.0 };

    HART_EXPECT_TRUE (first() (values.begin(), values.end()) == 3.0);
    HART_EXPECT_TRUE (last() (values.begin(), values.end()) == 2.0);
}

HART_TEST ("Reducers - Numeric")
{
    using hart::floatsEqual;
    using hart::max;
    using hart::mean;
    using hart::min;
    using hart::sum;

    const std::vector<double> values { 2.0, 4.0, 6.0, 8.0 };

    HART_EXPECT_TRUE (max() (values.begin(), values.end()) == 8.0);
    HART_EXPECT_TRUE (min() (values.begin(), values.end()) == 2.0);
    HART_EXPECT_TRUE (floatsEqual (mean() (values.begin(), values.end()), 5.0));
    HART_EXPECT_TRUE (floatsEqual (sum() (values.begin(), values.end()), 20.0));
}

HART_TEST ("Reducers - Collect")
{
    using hart::collect;

    const double nan = hart::nan<double>();
    const std::vector<double> values { 4.0, nan, -2.0 };

    const auto collected = collect() (values.begin(), values.end());

    HART_EXPECT_TRUE (collected.size() == 3);
    HART_EXPECT_TRUE (collected[0] == 4.0);
    HART_EXPECT_TRUE (std::isnan (collected[1]));
    HART_EXPECT_TRUE (collected[2] == -2.0);
}

HART_TEST ("Reducers - Boolean")
{
    using hart::allFloatsEqualToEachOther;
    using hart::allNaN;
    using hart::anyNaN;

    const double nan = hart::nan<double>();
    const std::vector<double> nanMixedValues { 4.0, nan, -2.0 };
    const std::vector<double> allNanValues { nan, nan };

    const std::vector<double> nearlyEqualValues { 1.0, 1.0005, 0.9995 };
    const std::vector<double> differentValues { 1.0, 1.02, 0.98 };

    HART_EXPECT_TRUE (anyNaN() (nanMixedValues.begin(), nanMixedValues.end()) == true);
    HART_EXPECT_TRUE (allNaN() (nanMixedValues.begin(), nanMixedValues.end()) == false);
    HART_EXPECT_TRUE (allNaN() (allNanValues.begin(), allNanValues.end()) == true);

    HART_EXPECT_TRUE (allFloatsEqualToEachOther() (nearlyEqualValues.begin(), nearlyEqualValues.end()) == false);
    HART_EXPECT_TRUE (allFloatsEqualToEachOther (0.001) (nearlyEqualValues.begin(), nearlyEqualValues.end()) == true);
    HART_EXPECT_TRUE (allFloatsEqualToEachOther (0.001) (differentValues.begin(), differentValues.end()) == false);
}
