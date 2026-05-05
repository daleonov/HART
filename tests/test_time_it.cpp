#include "hart.hpp"
#include "hart_time_it.hpp"
#include "dsp_render_time_mock.hpp"

HART_TEST ("TimeIt")
{
    const size_t blockSizeFrames = hart::CLIConfig::getInstance().getGefaultBlockSizeFrames();
    hart::AudioBuffer<float> mockBuffer (2, 10 * blockSizeFrames, 44100_Hz);

    constexpr double expectedRenderTimeSeconds = 0.0001_s;
    const double timeToRenderEachSampleSeconds = expectedRenderTimeSeconds / (mockBuffer.getNumFrames() * mockBuffer.getNumChannels());
    HART_ASSERT_FLOAT_NE (timeToRenderEachSampleSeconds, 0.0, 1_ns);
    DSPRenderTimeMock dspRenderTimeMock (timeToRenderEachSampleSeconds);

    // 1. Rendering in one go
    HART_TIME_IT (timeItRenderWholeBuffer, 1000)
    {
        mockBuffer.processWith (dspRenderTimeMock);
    }

    double meanObservedRenderTimeSeconds = timeItRenderWholeBuffer.result (hart::mean());
    HART_EXPECT_FLOAT_EQ (meanObservedRenderTimeSeconds, expectedRenderTimeSeconds, 0.3 * expectedRenderTimeSeconds);

    double p95ObservedRenderTimeSeconds = timeItRenderWholeBuffer.result (hart::percentile (0.95, hart::Interpolation::linear));
    HART_EXPECT_LT (p95ObservedRenderTimeSeconds, 2 * expectedRenderTimeSeconds);

    // 2. Rendering block-by-block
    HART_TIME_IT (timeItRenderBlockWise, 1000)
    {
        mockBuffer.processWith (dspRenderTimeMock, blockSizeFrames);
    }

    meanObservedRenderTimeSeconds = timeItRenderBlockWise.result (hart::mean());
    HART_EXPECT_FLOAT_EQ (meanObservedRenderTimeSeconds, expectedRenderTimeSeconds, 0.3 * expectedRenderTimeSeconds);

    p95ObservedRenderTimeSeconds = timeItRenderWholeBuffer.result (hart::percentile (0.95, hart::Interpolation::linear));
    HART_EXPECT_LT (p95ObservedRenderTimeSeconds, 2 * expectedRenderTimeSeconds);
}
