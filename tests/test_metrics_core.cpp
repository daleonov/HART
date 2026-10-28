#include "hart.hpp"
#include "mock_metric.hpp"

HART_DECLARE_ALIASES_FOR_FLOAT;
HART_DECLARE_ALIASES_FOR_UNITS;
using AudioBuffer = hart::AudioBuffer<float>;

// TODO: Move the existing metric query tests cases here

HART_TEST_WITH_TAGS ("MetricQuery - collect() just collects all the per-channel values", "[Metrics][MetricQuery]")
{
    const std::vector<int> values { 11, 22, 33 };
    const std::vector<int> extractedValues = mockMetric (values).collect();

    HART_EXPECT_EQ (extractedValues, values);
}
