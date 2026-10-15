#include <algorithm>  // max()
#include <cmath>  // abs(), isnan(), isinf(), cos(), acos(), tan()

#include "hart.hpp"
#include "exponential_decay.hpp"
#include "unstable_decay.hpp"

HART_DECLARE_ALIASES_FOR_FLOAT;
HART_DECLARE_ALIASES_FOR_UNITS;
using AudioBuffer = hart::AudioBuffer<float>;
using hart::floatsEqual;

HART_TEST ("Metrics - RT60 - Effect with exponentially decaying tail")
{
    using hart::rt60;
    using AudioBuffer = hart::AudioBuffer<float>;
    using ImpulseResponse = hart::ImpulseResponse<float>;

    const double defaultRenderDurationSeconds = hart::CLIConfig::getInstance().getDefaultRenderDurationSeconds();

    for (const double decayTimeSeconds : {1.234_ms, 10_ms, 42.42_ms, 777_ms})
    {
        AudioBuffer inputAudio;
        AudioBuffer outputAudio;

        processAudioWith (ExponentialDecay())
            .withValue (ExponentialDecay::decayTimeSeconds, decayTimeSeconds)
            .withInputSignal (Impulse())
            .inMono()
            .saveInputTo (inputAudio)
            .saveOutputTo (outputAudio)
            .withDuration (std::max (defaultRenderDurationSeconds, 1.5 * decayTimeSeconds))
            .process();
        
        const ImpulseResponse ir (inputAudio, outputAudio);
        HART_EXPECT_FLOAT_EQ (rt60 (ir).get(), rt60 (ir, hart::RT60::Method::edt).get(), 1.0e-8) << "Default method is EDT";

        const double toleranceSeconds = 0.01 * decayTimeSeconds;
        HART_EXPECT_FLOAT_EQ (rt60 (ir, hart::RT60::Method::edt).get(), decayTimeSeconds, toleranceSeconds) << "EDT method";
        HART_EXPECT_FLOAT_EQ (rt60 (ir, hart::RT60::Method::t20).get(), decayTimeSeconds, toleranceSeconds) << "T20 method";
        HART_EXPECT_FLOAT_EQ (rt60 (ir, hart::RT60::Method::t30).get(), decayTimeSeconds, toleranceSeconds) << "T30 method";
    }
}

HART_TEST ("Metrics - RT60 - Odd cases")
{
    using hart::rt60;
    using AudioBuffer = hart::AudioBuffer<float>;
    using ImpulseResponse = hart::ImpulseResponse<float>;

    AudioBuffer inputAudio;
    AudioBuffer outputAudio;

    // Silent output
    processAudioWith (Mute())
        .withInputSignal (Impulse())
        .saveInputTo (inputAudio)
        .saveOutputTo (outputAudio)
        .process();
    
    ImpulseResponse ir (inputAudio, outputAudio);
    HART_EXPECT_IS_NAN (rt60 (ir).get()) << "Silent output - RT60 reads as NaN";

    // No decay
    processAudioWith (GainDb (-3_dB))
        .withInputSignal (Impulse())
        .saveInputTo (inputAudio)
        .saveOutputTo (outputAudio)
        .process();
    ir = ImpulseResponse (inputAudio, outputAudio);
    HART_EXPECT_IS_NAN (rt60 (ir).get()) << "No decay - RT60 reads as NaN";

    // Growing response, instead of decaying to zero.
    // RT60 assumes a decaying IR, so this example kind of breaks it.
    // It's recommended to verify a correct response shape/slope before using RT60.
    processAudioWith (UnstableDecay())
        .withInputSignal (Impulse())
        .saveInputTo (inputAudio)
        .saveOutputTo (outputAudio)
        .process();
    ir = ImpulseResponse (inputAudio, outputAudio);
    HART_EXPECT_NOT_NAN (rt60 (ir).get()) << "Growing response - RT60 reads as a seemily valid value";
}

HART_TEST ("Metrics - RT60 - Units")
{
    using hart::rt60;
    using AudioBuffer = hart::AudioBuffer<float>;
    using ImpulseResponse = hart::ImpulseResponse<float>;

    const double sampleRateHz = hart::CLIConfig::getInstance().getDefaultSampleRateHz();
    const double defaultRenderDurationSeconds = hart::CLIConfig::getInstance().getDefaultRenderDurationSeconds();
    const double expectedDecayTimeSeconds = 50_ms;
    const double expectedDecayTimeFrames = expectedDecayTimeSeconds * sampleRateHz;

    AudioBuffer inputAudio;
    AudioBuffer outputAudio;

    processAudioWith (ExponentialDecay())
        .withValue (ExponentialDecay::decayTimeSeconds, expectedDecayTimeSeconds)
        .withInputSignal (Impulse())
        .inMono()
        .saveInputTo (inputAudio)
        .saveOutputTo (outputAudio)
        .withDuration (std::max (defaultRenderDurationSeconds, 1.5 * expectedDecayTimeSeconds))
        .process();
    
    const ImpulseResponse ir (inputAudio, outputAudio);
    HART_EXPECT_FLOAT_EQ (rt60 (ir).as (seconds).get(), rt60 (ir).get(), 1.0e-8) << "Default unit is seconds";
    HART_EXPECT_FLOAT_EQ (rt60 (ir).as (seconds).get(), rt60 (ir).as (native).get(), 1.0e-8) << "Native unit is seconds";
    HART_EXPECT_FLOAT_EQ (rt60 (ir).as (seconds).get(), expectedDecayTimeSeconds, 100_us) << "Seconds are indeed interpreted as seconds";
    HART_EXPECT_FLOAT_EQ (rt60 (ir).as (frames).get(), expectedDecayTimeFrames, 1.0) << "Supports returning decay time in frames";
}

HART_TEST ("Metrics - Centre Time - Effect with exponentially decaying tail")
{
    using hart::centreTime;
    using hart::rt60;
    using AudioBuffer = hart::AudioBuffer<float>;
    using ImpulseResponse = hart::ImpulseResponse<float>;

    const double defaultRenderDurationSeconds = hart::CLIConfig::getInstance().getDefaultRenderDurationSeconds();

    for (const double rt60Seconds : {1.234_ms, 10_ms, 42.42_ms, 777_ms})
    {
        AudioBuffer inputAudio;
        AudioBuffer outputAudio;

        processAudioWith (ExponentialDecay())
            .withValue (ExponentialDecay::decayTimeSeconds, rt60Seconds)
            .withInputSignal (Impulse())
            .inMono()
            .saveInputTo (inputAudio)
            .saveOutputTo (outputAudio)
            .withDuration (std::max (defaultRenderDurationSeconds, 1.5 * rt60Seconds))
            .process();
        
        const ImpulseResponse ir (inputAudio, outputAudio);
        const double toleranceSeconds = 0.01 * rt60Seconds;
        HART_ASSERT_FLOAT_EQ (rt60 (ir).get(), rt60Seconds, toleranceSeconds) << "Effect has a correct RT60 decay time";

        constexpr double k = 0.072382413650542;  // 1 / (6 * ln (10));
        const double expectedCentreTimeSeconds = k * rt60Seconds;

        HART_EXPECT_FLOAT_EQ (centreTime (ir).get(), expectedCentreTimeSeconds, toleranceSeconds) << "Centre time at RT60 = " << rt60Seconds;
    }
}

HART_TEST ("Metrics - Centre Time - Odd cases")
{
    using hart::centreTime;
    using AudioBuffer = hart::AudioBuffer<float>;
    using ImpulseResponse = hart::ImpulseResponse<float>;

    AudioBuffer inputAudio;
    AudioBuffer outputAudio;

    // Silent output
    processAudioWith (Mute())
        .withInputSignal (Impulse())
        .saveInputTo (inputAudio)
        .saveOutputTo (outputAudio)
        .process();
    
    ImpulseResponse ir (inputAudio, outputAudio);
    HART_EXPECT_IS_NAN (centreTime (ir).get()) << "Silent output - Centre Time reads as NaN";

    // No decay
    processAudioWith (GainDb (-3_dB))
        .withInputSignal (Impulse())
        .saveInputTo (inputAudio)
        .saveOutputTo (outputAudio)
        .process();

    ir = ImpulseResponse (inputAudio, outputAudio);
    HART_EXPECT_FLOAT_EQ (centreTime (ir).get(), 0_s, 1_us) << "No decay - Centre Time reads as zero";
}

HART_TEST ("Metrics - Centre Time - Units")
{
    using hart::centreTime;
    using AudioBuffer = hart::AudioBuffer<float>;
    using ImpulseResponse = hart::ImpulseResponse<float>;

    const double sampleRateHz = hart::CLIConfig::getInstance().getDefaultSampleRateHz();
    const double defaultRenderDurationSeconds = hart::CLIConfig::getInstance().getDefaultRenderDurationSeconds();
    const double rt60Seconds = 50_ms;
    
    constexpr double k = 0.072382413650542;  // 1 / (6 * ln (10));
    const double expectedCentreTimeSeconds = k * rt60Seconds;
    const double expectedCentreTimeFrames = expectedCentreTimeSeconds * sampleRateHz;

    AudioBuffer inputAudio;
    AudioBuffer outputAudio;

    processAudioWith (ExponentialDecay())
        .withValue (ExponentialDecay::decayTimeSeconds, rt60Seconds)
        .withInputSignal (Impulse())
        .inMono()
        .saveInputTo (inputAudio)
        .saveOutputTo (outputAudio)
        .withDuration (std::max (defaultRenderDurationSeconds, 1.5 * rt60Seconds))
        .process();
    
    const ImpulseResponse ir (inputAudio, outputAudio);
    HART_EXPECT_FLOAT_EQ (centreTime (ir).as (seconds).get(), centreTime (ir).get(), 1.0e-8) << "Default unit is seconds";
    HART_EXPECT_FLOAT_EQ (centreTime (ir).as (seconds).get(), centreTime (ir).as (native).get(), 1.0e-8) << "Native unit is seconds";
    HART_EXPECT_FLOAT_EQ (centreTime (ir).as (seconds).get(), expectedCentreTimeSeconds, 100_us) << "Seconds are indeed interpreted as seconds";
    HART_EXPECT_FLOAT_EQ (centreTime (ir).as (frames).get(), expectedCentreTimeFrames, 1.0) << "Supports returning decay time in frames";
}

