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

HART_TEST ("AudioBuffer - Multiply Buffer by a Number")
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

    HART_EXPECT_NE (audioBufferA, audioBufferA * 0.5f);
    HART_EXPECT_NE (audioBufferC, audioBufferC * -1.0f);

    HART_EXPECT_EQ (audioBufferA * 1.0f, audioBufferA);
    HART_EXPECT_EQ (audioBufferB * 2.0f, audioBufferB * 2.0f);

    HART_EXPECT_EQ (audioBufferB * (1.0f / 0.4f), audioBufferA);
    HART_EXPECT_EQ (audioBufferC * -1.0f, audioBufferD);
    HART_EXPECT_EQ (audioBufferB * 1.5f, audioBufferC);
    HART_EXPECT_EQ (audioBufferD * (-1.0f / 0.6f), audioBufferA);

    HART_EXPECT_EQ (audioBufferA * 0.0f, audioBufferE);
    HART_EXPECT_EQ (audioBufferB * 0.0f, audioBufferB * 0.0f);
    HART_EXPECT_EQ (audioBufferE * 123.456f, audioBufferE);
}
