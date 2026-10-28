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

HART_TEST_WITH_TAGS ("Windowed MetricQuery - get() picks correct values with default axis", "[Metrics][WindowedMetricQuery]")
{
    using hart::windowed;
    using hart::max;
    using hart::min;
    using hart::roundToSizeT;

    const std::vector<std::vector<int>> values = {
        { 11, 22, 33 },
        { 444, 555, 666 },
        { 7777, 8888, 9999 }
    };

    const double sampleRateHz = 44100_Hz;
    const size_t dummyBufferSizeFrames = roundToSizeT (sampleRateHz * 300_ms);
    const AudioBuffer dummyBuffer (3, dummyBufferSizeFrames, sampleRateHz);
    auto mockMetric = mockWindowedMetric (values);

    HART_EXPECT_EQ (
        windowed (dummyBuffer)
            .metric ([mockMetric] (const AudioBuffer&) mutable { return mockMetric().collect(); })
            .get (min(), min()),
        11
        );

    HART_EXPECT_EQ (
        windowed (dummyBuffer)
            .metric ([mockMetric] (const AudioBuffer&) mutable { return mockMetric().collect(); })
            .get (max(), max()),
        9999
        );

    HART_EXPECT_EQ (
        windowed (dummyBuffer)
            .metric ([mockMetric] (const AudioBuffer&) mutable { return mockMetric().collect(); })
            .get (max(), min()),
        7777
        );

    HART_EXPECT_EQ (
        windowed (dummyBuffer)
            .metric ([mockMetric] (const AudioBuffer&) mutable { return mockMetric().collect(); })
            .get (min(), max()),
        33
        );
}

HART_TEST_WITH_TAGS ("Windowed MetricQuery - get() accepts non-default axes", "[Metrics][WindowedMetricQuery]")
{
    using hart::Axis;
    using hart::windowed;
    using hart::max;
    using hart::min;
    using hart::roundToSizeT;

    const std::vector<std::vector<int>> values = {
        { 11, 22, 33 },
        { 444, 555, 666 },
        { 7777, 8888, 9999 }
    };

    const double sampleRateHz = 44100_Hz;
    const size_t dummyBufferSizeFrames = roundToSizeT (sampleRateHz * 300_ms);
    const AudioBuffer dummyBuffer (3, dummyBufferSizeFrames, sampleRateHz);
    auto mockMetric = mockWindowedMetric (values);

    HART_EXPECT_EQ (
        windowed (dummyBuffer)
            .metric ([mockMetric] (const AudioBuffer&) mutable { return mockMetric().collect(); })
            .get (min(), max(), Axis::channels),
        33
        ) << "Second reducer is applied to each channel";

    HART_EXPECT_EQ (
        windowed (dummyBuffer)
            .metric ([mockMetric] (const AudioBuffer&) mutable { return mockMetric().collect(); })
            .get (min(), max(), Axis::frames),
        7777
        ) << "Second reducer is applied to each window";

    HART_EXPECT_EQ (
        windowed (dummyBuffer)
            .metric ([mockMetric] (const AudioBuffer&) mutable { return mockMetric().collect(); })
            .get (min(), max()),
        33
        ) << "Default axis is Axis::channels";
}
