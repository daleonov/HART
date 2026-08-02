#include "hart.hpp"
#include "simple_fir_filter.hpp"

HART_DECLARE_ALIASES_FOR_FLOAT;
using AudioBuffer = hart::AudioBuffer<float>;
using ImpulseResponse = hart::ImpulseResponse<float>;

HART_TEST ("ImpulseResponse - Has correct number of frames")
{
    for (const double durationSeconds : {123_us, 1_ms, 4.56_ms, 78.9_ms})
    {
        AudioBuffer inputAudio;
        AudioBuffer outputAudio;

        processAudioWith (GainDb (0_dB))
            .withInputSignal (Impulse())
            .withDuration (durationSeconds)
            .saveInputTo (inputAudio)
            .saveOutputTo (outputAudio)
            .process();
        
        HART_ASSERT_GT (inputAudio.getNumFrames(), 0);
        HART_ASSERT_GT (outputAudio.getNumFrames(), 0);
        HART_ASSERT_EQ (inputAudio.getNumFrames(), outputAudio.getNumFrames());

        const ImpulseResponse observedIr (inputAudio, outputAudio);
        HART_EXPECT_EQ (observedIr.getNumFrames(), inputAudio.getNumFrames());
    }
}

HART_TEST ("ImpulseResponse - Has correct number of channels")
{
    for (const size_t numChannels : {1, 2, 5, 15})
    {
        AudioBuffer inputAudio;
        AudioBuffer outputAudio;

        processAudioWith (GainDb (0_dB))
            .withInputSignal (Impulse())
            .withDuration (1_ms)
            .withInputChannels (numChannels)
            .withOutputChannels (numChannels)
            .saveInputTo (inputAudio)
            .saveOutputTo (outputAudio)
            .process();
        
        HART_ASSERT_EQ (inputAudio.getNumChannels(), numChannels);
        HART_ASSERT_EQ (outputAudio.getNumChannels(), numChannels);

        const ImpulseResponse observedIr (inputAudio, outputAudio);
        HART_EXPECT_EQ (observedIr.getNumChannels(), numChannels);
    }
}

HART_TEST ("ImpulseResponse - Has correct sample rate")
{
    for (const double sampleRateHz : {5_kHz, 44100_Hz, 48_kHz, 96_kHz, 196_kHz})
    {
        AudioBuffer inputAudio;
        AudioBuffer outputAudio;

        processAudioWith (GainDb (0_dB))
            .withInputSignal (Impulse())
            .withDuration (1_ms)
            .withSampleRate (sampleRateHz)
            .saveInputTo (inputAudio)
            .saveOutputTo (outputAudio)
            .process();
        
        HART_ASSERT_TRUE (inputAudio.hasSampleRate());
        HART_ASSERT_TRUE (outputAudio.hasSampleRate());
        HART_ASSERT_FLOAT_EQ (inputAudio.getSampleRateHz(), sampleRateHz, 1.0e-8);
        HART_ASSERT_FLOAT_EQ (outputAudio.getSampleRateHz(), sampleRateHz, 1.0e-8);

        const ImpulseResponse observedIr (inputAudio, outputAudio);
        HART_ASSERT_FLOAT_EQ (observedIr.getSampleRateHz(), sampleRateHz, 1.0e-8);
    }
}

HART_TEST ("ImpulseResponse - Matches expected FIR filter's coefficients")
{
    AudioBuffer inputAudio;
    AudioBuffer outputAudio;

    processAudioWith (SimpleFIRFilter())
        .withInputSignal (Impulse())
        .inMono()
        .withDuration (1_ms)
        .saveInputTo (inputAudio)
        .saveOutputTo (outputAudio)
        .process();

    const ImpulseResponse observedIr (inputAudio, outputAudio);
    HART_ASSERT_EQ (observedIr.getNumChannels(), 1);
    HART_ASSERT_GE (observedIr.getNumFrames(), 3);

    constexpr float epsilon = 1.0e-8f;
    const float* observedIrData = observedIr[hart::Channel::left];

    HART_EXPECT_FLOAT_EQUAL (observedIrData[0], 0.123f, epsilon) << "0th frame in the IR matches filter's x[n] coefficient";
    HART_EXPECT_FLOAT_EQUAL (observedIrData[1], 0.456f, epsilon) << "1th frame in the IR matches filter's x[n - 1] coefficient";

    for (int frame = 2; frame < observedIr.getNumFrames(); ++frame)
        HART_EXPECT_FLOAT_EQUAL (observedIrData[frame], 0.0f, epsilon) << "Frame " << frame << " in the IR is zero";
}
