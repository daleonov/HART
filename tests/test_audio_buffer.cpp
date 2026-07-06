#include "hart.hpp"

HART_DECLARE_ALIASES_FOR_FLOAT;
using AudioBuffer = hart::AudioBuffer<float>;

HART_TEST ("AudioBuffer - Add Two Buffers")
{
    const hart::CLIConfig& cliConfig = hart::CLIConfig::getInstance();
    const double sampleRateHz = cliConfig.getDefaultSampleRateHz();
    const double renderDurationSeconds = cliConfig.getDefaultRenderDurationSeconds();
    const size_t numFrames = hart::roundToSizeT (renderDurationSeconds * sampleRateHz);
    const size_t numChannels = cliConfig.getDefaultNumInputChannels();

    AudioBuffer audioBufferA = AudioBuffer (numChannels, numFrames, sampleRateHz).fillWith (WhiteNoise());
    AudioBuffer audioBufferB = AudioBuffer (numChannels, numFrames, sampleRateHz).fillWith (WhiteNoise() >> GainLinear (0.4));
    AudioBuffer audioBufferC = AudioBuffer (numChannels, numFrames, sampleRateHz).fillWith (WhiteNoise() >> GainLinear (0.6));
    AudioBuffer audioBufferD = AudioBuffer (numChannels, numFrames, sampleRateHz).fillWith (WhiteNoise() >> GainLinear (-0.6));
    AudioBuffer audioBufferE = AudioBuffer (numChannels, numFrames, sampleRateHz).fillWith (Silence());

    HART_EXPECT_NE (audioBufferA, audioBufferB);
    HART_EXPECT_NE (audioBufferB, audioBufferC);
    HART_EXPECT_NE (audioBufferC, audioBufferD);
    HART_EXPECT_NE (audioBufferD, audioBufferE);

    HART_EXPECT_EQ (audioBufferB + audioBufferC, audioBufferA);
    HART_EXPECT_EQ (audioBufferC + audioBufferD, audioBufferE);
}
