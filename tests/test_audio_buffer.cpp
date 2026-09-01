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

HART_TEST ("AudioBuffer - Subtract Two Buffers")
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

    HART_EXPECT_NE (audioBufferA - audioBufferA, audioBufferA);
    HART_EXPECT_NE (audioBufferA - audioBufferB, audioBufferA);
    HART_EXPECT_NE (audioBufferB - audioBufferC, audioBufferD);
    HART_EXPECT_NE (audioBufferE - audioBufferA, audioBufferA);

    HART_EXPECT_EQ (audioBufferA - audioBufferB, audioBufferC);
    HART_EXPECT_EQ (audioBufferE - audioBufferC, audioBufferD);
    HART_EXPECT_EQ (audioBufferE - audioBufferD, audioBufferC);
    HART_EXPECT_EQ (audioBufferB - audioBufferD, audioBufferA);
    HART_EXPECT_EQ (audioBufferA - audioBufferE, audioBufferA);
    HART_EXPECT_EQ (audioBufferA - audioBufferA, audioBufferE);
    HART_EXPECT_EQ (audioBufferB - audioBufferB, audioBufferC - audioBufferC);
    HART_EXPECT_EQ (audioBufferA - audioBufferB, audioBufferC - audioBufferE);
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

HART_TEST ("AudioBuffer - Fill with a specific value")
{
    const hart::CLIConfig& cliConfig = hart::CLIConfig::getInstance();
    const double sampleRateHz = cliConfig.getDefaultSampleRateHz();
    const double renderDurationSeconds = cliConfig.getDefaultRenderDurationSeconds();
    const size_t numFrames = hart::roundToSizeT (renderDurationSeconds * sampleRateHz);
    const size_t numChannels = cliConfig.getDefaultNumInputChannels();

    constexpr float value = 123.456f;
    const auto buffer = AudioBuffer (numChannels, numFrames, sampleRateHz).fillWith (value);

    for (size_t channel = 0; channel < numChannels; ++channel)
    {
        const float* channelData = buffer[channel];

        for (size_t frame = 0; frame < numFrames; ++frame)
            HART_EXPECT_FLOAT_EQ (channelData[frame], value, 1e-8f) << "Channel " << channel << ", frame " << frame;
    }
}

HART_TEST ("AudioBuffer - Resampling")
{
    using hart::floatsNotEqual;
    using hart::quinns2;
    using hart::esr;
    using hart::max;
    using hart::nth;
    using Channel = hart::Channel;

    constexpr double originalSampleRateHz = 44.1_kHz;

    const auto leftChannel = nth (Channel::left);
    const auto rightChannel = nth (Channel::right);

    // Left: 1 kHz, Right: 440 Hz
    auto inputSignal = (Sawtooth (440_Hz) >> Mute().atChannel (Channel::left)) + (Sawtooth (1_kHz) >> Mute().atChannel (Channel::right));
    const auto bufferA = AudioBuffer (2, 10000, originalSampleRateHz).fillWith (inputSignal);
    HART_ASSERT_FREQUENCIES_EQUAL (quinns2 (bufferA).get (leftChannel), 1_kHz, 5_cents);
    HART_ASSERT_FREQUENCIES_EQUAL (quinns2 (bufferA).get (rightChannel), 440_Hz, 5_cents);

    const double bufferALengthSeconds = bufferA.getLengthSeconds();
    const double bufferLengthToleranceSeconds = 1e-4 * bufferALengthSeconds;

    for (const double targetSampleRateHz : { 8_kHz, 16_kHz, 32_kHz, 48_kHz, 88.2_kHz, 96_kHz, 196_kHz })
    {
        HART_ASSERT_FLOAT_NE (targetSampleRateHz, originalSampleRateHz, 1e-16)
            << "Resampling to identical SR is tested separately";

        const AudioBuffer bufferB = bufferA.resample (targetSampleRateHz);
        const std::string labelPrefix = HART_STR ("Resampled buffer at " << targetSampleRateHz << " Hz ");

        HART_EXPECT_TRUE (bufferB.hasSampleRate())
            << labelPrefix << "should have SR properly assigned";

        HART_EXPECT_FLOAT_EQ (bufferB.getSampleRateHz(), targetSampleRateHz, 1e-16)
            << labelPrefix << "should have correct SR";

        HART_EXPECT_FLOAT_NE (bufferB.getSampleRateHz(), bufferA.getSampleRateHz(), 1e-16)
            << labelPrefix << "should have different SR from the source after resampling";

        HART_EXPECT_FLOAT_EQ (bufferB.getLengthSeconds(), bufferALengthSeconds, bufferLengthToleranceSeconds)
            << labelPrefix << "should retain the source's length in seconds";

        HART_EXPECT_NE (bufferB.getNumFrames(), bufferA.getNumFrames())
            << labelPrefix << "should have a different length in frames from the source";

        HART_EXPECT_EQ (bufferB.getNumChannels(), bufferA.getNumChannels())
            << labelPrefix << "should retain correct number of channels";

        HART_EXPECT_FREQUENCIES_EQUAL (quinns2 (bufferB).get (leftChannel), 1_kHz, 5_cents)
            << labelPrefix << "should retain correct fundamental frequency at the left channel";
    
        HART_EXPECT_FREQUENCIES_EQUAL (quinns2 (bufferB).get (rightChannel), 440_Hz, 5_cents)
            << labelPrefix << "should retain correct fundamental frequency at the right channel";

        const AudioBuffer bufferC = bufferB.resample (originalSampleRateHz);
        HART_ASSERT_EQ (bufferC.getNumFrames(), bufferA.getNumFrames());

        const float maxAcceptableEsr = targetSampleRateHz < originalSampleRateHz ? 0.2 : 1e-3;
        HART_EXPECT_LT (esr (bufferC, bufferA). get (max()), maxAcceptableEsr)
            << labelPrefix << "should re-resample back to the original SR without losing too much fidelity";
    }

    // Edge case - Resampling to same SR
    const AudioBuffer bufferB = bufferA.resample (originalSampleRateHz);
    HART_ASSERT_EQUAL (bufferA, bufferB)
        << "Resampled buffer is identical (within a threshold, but actually bit-identical) to original after resampling to identical SR";
}

HART_TEST ("AudioBuffer - makeCopyOf() - matching sample type")
{
    AudioBuffer bufferA (2, 3, 12345_Hz);
    bufferA.clear();
    bufferA[0][0] = 1.111f;
    bufferA[0][1] = 2.222f;
    bufferA[0][2] = 3.333f;
    bufferA[1][0] = 4.444f;
    bufferA[1][1] = 5.555f;
    bufferA[1][2] = 6.666f;

    AudioBuffer bufferB (5, 6, 45678_Hz);
    bufferB.makeCopyOf (bufferA);

    HART_EXPECT_EQ (bufferA, bufferB);
}

HART_TEST ("AudioBuffer - makeCopyOf() - different sample types")
{
    hart::AudioBuffer<double> bufferA (2, 3, 12345_Hz);
    bufferA.clear();
    bufferA[0][0] = 1.111;
    bufferA[0][1] = 2.222;
    bufferA[0][2] = 3.333;
    bufferA[1][0] = 4.444;
    bufferA[1][1] = 5.555;
    bufferA[1][2] = 6.666;

    hart::AudioBuffer<float> bufferB (5, 6, 45678_Hz);
    bufferB.makeCopyOf (bufferA);

    HART_EXPECT_EQ (bufferA.getNumChannels(), bufferB.getNumChannels());
    HART_EXPECT_EQ (bufferA.getNumFrames(), bufferB.getNumFrames());
    HART_EXPECT_FLOAT_EQ (bufferA.getSampleRateHz(), bufferB.getSampleRateHz(), 1e-12);
    HART_EXPECT_FLOAT_EQ (bufferA[0][0], static_cast<double>(bufferB[0][0]), 1e-6);
    HART_EXPECT_FLOAT_EQ (bufferA[0][1], static_cast<double>(bufferB[0][1]), 1e-6);
    HART_EXPECT_FLOAT_EQ (bufferA[0][2], static_cast<double>(bufferB[0][2]), 1e-6);
    HART_EXPECT_FLOAT_EQ (bufferA[1][0], static_cast<double>(bufferB[1][0]), 1e-6);
    HART_EXPECT_FLOAT_EQ (bufferA[1][1], static_cast<double>(bufferB[1][1]), 1e-6);
    HART_EXPECT_FLOAT_EQ (bufferA[1][2], static_cast<double>(bufferB[1][2]), 1e-6);
    hart::AudioBuffer<double> bufferC (1, 4, 54321_Hz);
    bufferC.makeCopyOf (bufferB);

    HART_ASSERT_EQ (bufferC, bufferA) << "f64 - f32 - f64 round-trip matches the original buffer";
}

HART_TEST ("AudioBuffer - copyFrom() for entire matched AudioBuffer")
{
    AudioBuffer bufferA (2, 3, 42424_Hz);
    bufferA[0][0] = 1.111f;
    bufferA[0][1] = 2.222f;
    bufferA[0][2] = 3.333f;
    bufferA[1][0] = 4.444f;
    bufferA[1][1] = 5.555f;
    bufferA[1][2] = 6.666f;
    
    AudioBuffer bufferB (2, 3, 42424_Hz);
    bufferB.copyFrom (bufferA);

    HART_ASSERT_EQ (bufferA, bufferB);
}
