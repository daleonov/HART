#include <cstdlib>  // rand()

#include "hart.hpp"

HART_DECLARE_ALIASES_FOR_FLOAT;

HART_TEST ("Detecting denormals")
{
    processAudioWith (GainDb())
        .withLabel ("Unity gain")
        .withInputSignal (SineWave())
        .expectTrue (NoDenormals())
        .process();

    processAudioWith (GainLinear (1e-38))
        .withLabel ("-760 dB - All denormals")
        .withInputSignal (SineWave())
        .expectFalse (NoDenormals())
        .process();

    processAudioWith (GainLinear (1e-30))
        .withLabel ("-600 dB - Some denormals")
        .withInputSignal (SineWave())
        .expectFalse (NoDenormals())
        .expectFalse (EqualsTo (Silence(), 1e-36))
        .process();

    processAudioWith (GainLinear (1e-25))
        .withLabel ("-500 dB - Occasional denormals")
        .withInputSignal (SineWave())
        .expectFalse (NoDenormals())
        .expectFalse (EqualsTo (Silence(), 1e-36))
        .process();

    processAudioWith (GainLinear (0))
        .withLabel ("Complete silence")
        .withInputSignal (SineWave())
        .expectTrue (NoDenormals())
        .expectTrue (EqualsTo (Silence(), 1e-36))
        .process();
}

HART_TEST ("Matcher function - For output buffer")
{
    using AudioBuffer = hart::AudioBuffer<float>;

    processAudioWith (GainDb (0_dB))
        .withLabel ("Creating MatcherFunction explicitly")
        .withInputSignal (SineWave())
        .expectTrue (MatcherFunction ([] (const AudioBuffer&) { return true; }, "Always true"))
        .expectFalse (MatcherFunction ([] (const AudioBuffer&) { return false; }, "Always false"))
        .process();

    auto peaksAboveUnity = MatcherFunction (
        [] (const AudioBuffer& output) { return output.getMagnitude (0, output.getNumFrames()) > 1.0f; },
        "Peaks above unity"
        );

    processAudioWith (GainDb (0_dB))
        .withLabel ("Using a named object")
        .withInputSignal (SineWave() >> GainDb (3_dB))
        .expectTrue (std::move (peaksAboveUnity))
        .process();

    processAudioWith (GainDb (0_dB))
        .withLabel ("Creating MatcherFunction implicitly")
        .withInputSignal (SineWave())
        .expectTrue ([] (const AudioBuffer&) { return true; }, "Always true")
        .expectFalse ([] (const AudioBuffer&) { return false; }, "Always false")
        .process();
}

HART_TEST ("Matcher function - For input and output buffers")
{
    using AudioBuffer = hart::AudioBuffer<float>;

    processAudioWith (GainDb (0_dB))
        .withLabel ("Creating MatcherFunction explicitly")
        .withInputSignal (SineWave())
        .expectTrue (MatcherFunction ([] (const AudioBuffer&, const AudioBuffer&) { return true; }, "Always true"))
        .expectFalse (MatcherFunction ([] (const AudioBuffer&, const AudioBuffer&) { return false; }, "Always false"))
        .process();

    processAudioWith (GainDb (0_dB))
        .withLabel ("Creating MatcherFunction implicitly")
        .withInputSignal (SineWave())
        .expectTrue ([] (const AudioBuffer&, const AudioBuffer&) { return true; }, "Always true")
        .expectFalse ([] (const AudioBuffer&, const AudioBuffer&) { return false; }, "Always false")
        .process();
}

HART_TEST ("LatencyBelow - Onset method")
{
    using Method = LatencyBelow::Method;

    processAudioWith (GainDb (0_dB))
        .withLabel ("No latency")
        .withInputSignal (SineWave())
        .expectTrue (LatencyBelow (100_us, Method::onset))
        .process();

    processAudioWith (TimeShift (5_ms))
        .withLabel ("Impulse into TimeShift - mono")
        .withInputSignal (Impulse())
        .expectTrue (LatencyBelow (5.1_ms, Method::onset))
        .expectFalse (LatencyBelow (4.9_ms, Method::onset))
        .process();

    processAudioWith (TimeShift (5_ms))
        .withLabel ("Impulse into TimeShift - 5 channels")
        .withInputChannels (5)
        .withOutputChannels (5)
        .withInputSignal (Impulse())
        .expectTrue (LatencyBelow (5.1_ms, Method::onset))
        .expectFalse (LatencyBelow (4.9_ms, Method::onset))
        .process();

    using hart::roundToSizeT;
    using hart::Loop;
    using AudioBuffer = hart::AudioBuffer<float>;

    SignalFunction delayedImpulse (
        [] (AudioBuffer& buffer) {
            constexpr double impulseTimingSeconds = 0.05_s;
            const size_t impulseTimingFrames =
                1 + roundToSizeT (impulseTimingSeconds * buffer.getSampleRateHz());

            buffer.setNumFrames (impulseTimingFrames);
            buffer.clear();

            for (size_t channel = 0; channel < buffer.getNumChannels(); ++channel)
                buffer[channel][buffer.getNumFrames() - 1] = 1.0f;
        },
        "Delayed impulse",
        Loop::no
        );

    processAudioWith (TimeShift (5_ms))
        .withLabel ("Delayed impulse into TimeShift")
        .withInputSignal (std::move (delayedImpulse))
        .withDuration (70_ms)  // Delayed impulse timing + expected latency + a little bit on top
        .expectTrue (LatencyBelow (5.1_ms, Method::onset))
        .expectFalse (LatencyBelow (4.9_ms, Method::onset))
        .process();

    auto percussiveBurstEnvelope = SegmentedEnvelope (-60_dB)
        .hold (10_ms)
        .rampTo (0_dB, 1_ms)
        .hold (30_ms)
        .rampTo (0_dB, 20_ms);

    processAudioWith (TimeShift (5_ms))
        .withLabel ("WhiteNoise burst into TimeShift")
        .withInputSignal (WhiteNoise() >> GainDb().withEnvelope (GainDb::gainDb, percussiveBurstEnvelope))
        .expectTrue (LatencyBelow (5.1_ms, Method::onset))
        .expectFalse (LatencyBelow (4.9_ms, Method::onset))
        .process();

    processAudioWith (TimeShift (5_ms))
        .withLabel ("Threshold option")
        .withInputSignal (Impulse() >> GainDb (-10_dB))
        .expectTrue (LatencyBelow (5.1_ms, Method::onset, -30_dB))
        .expectFalse (LatencyBelow (5.1_ms, Method::onset, -3_dB))
        .process();

    using SilencePolicy = hart::SilencePolicy;
    processAudioWith (TimeShift (5_ms))
        .withLabel ("Silence policy option")
        .withInputSignal (Impulse() >> GainLinear (0).atChannel (hart::Channel::left))
        .inStereo()
        .expectTrue (LatencyBelow (5.1_ms, Method::onset, -120_dB, SilencePolicy::relaxed))
        .expectFalse (LatencyBelow (5.1_ms, Method::onset, -120_dB, SilencePolicy::strict))
        .process();
}

HART_TEST ("LatencyBelow - Correlation method")
{
    using Method = LatencyBelow::Method;

    const unsigned int randomSeed = static_cast<unsigned int> (hart::CLIConfig::getInstance().getRandomSeed());
    std::srand (randomSeed);

    processAudioWith (GainDb (0_dB))
        .withLabel ("No latency")
        .withInputSignal (SineSweep())
        .expectTrue (LatencyBelow (100_us, Method::correlation))
        .process();

    processAudioWith (TimeShift (5_ms))
        .withLabel ("SineSweep into TimeShift - mono")
        .withInputSignal (SineSweep())
        .expectTrue (LatencyBelow (5.1_ms, Method::correlation))
        .expectFalse (LatencyBelow (4.9_ms, Method::correlation))
        .process();

    processAudioWith (TimeShift (5_ms))
        .withLabel ("SineSweep into TimeShift - 5 channels")
        .withInputChannels (5)
        .withOutputChannels (5)
        .withInputSignal (SineSweep())
        .expectTrue (LatencyBelow (5.1_ms, Method::correlation))
        .expectFalse (LatencyBelow (4.9_ms, Method::correlation))
        .process();

    processAudioWith (TimeShift (5_ms))
        .withLabel ("Impulse into TimeShift - mono")
        .withInputSignal (Impulse())
        .expectTrue (LatencyBelow (5.1_ms, Method::correlation))
        .expectFalse (LatencyBelow (4.9_ms, Method::correlation))
        .process();

    using hart::roundToSizeT;
    using hart::Loop;
    using AudioBuffer = hart::AudioBuffer<float>;

    SignalFunction delayedImpulse (
        [] (AudioBuffer& buffer) {
            constexpr double impulseTimingSeconds = 0.05_s;
            const size_t impulseTimingFrames =
                1 + roundToSizeT (impulseTimingSeconds * buffer.getSampleRateHz());

            buffer.setNumFrames (impulseTimingFrames);
            buffer.clear();

            for (size_t channel = 0; channel < buffer.getNumChannels(); ++channel)
                buffer[channel][buffer.getNumFrames() - 1] = 1.0f;
        },
        "Delayed impulse",
        Loop::no
        );

    processAudioWith (TimeShift (5_ms))
        .withLabel ("Delayed impulse into TimeShift")
        .withInputSignal (std::move (delayedImpulse))
        .withDuration (70_ms)  // Delayed impulse timing + expected latency + a little bit on top
        .expectTrue (LatencyBelow (5.1_ms, Method::correlation))
        .expectFalse (LatencyBelow (4.9_ms, Method::correlation))
        .process();

    auto percussiveBurstEnvelope = SegmentedEnvelope (-60_dB)
        .hold (10_ms)
        .rampTo (0_dB, 1_ms)
        .hold (30_ms)
        .rampTo (0_dB, 20_ms);

    processAudioWith (TimeShift (5_ms))
        .withLabel ("WhiteNoise burst into TimeShift")
        .withInputSignal (WhiteNoise() >> GainDb().withEnvelope (GainDb::gainDb, percussiveBurstEnvelope))
        .expectTrue (LatencyBelow (5.1_ms, Method::correlation))
        .expectFalse (LatencyBelow (4.9_ms, Method::correlation))
        .process();

    using SilencePolicy = hart::SilencePolicy;
    processAudioWith (TimeShift (5_ms))
        .withLabel ("Silence policy option")
        .withInputSignal (Impulse() >> GainLinear (0).atChannel (hart::Channel::left))
        .inStereo()
        .expectTrue (LatencyBelow (5.1_ms, Method::correlation, 0.5, SilencePolicy::relaxed))
        .expectFalse (LatencyBelow (5.1_ms, Method::correlation, 0.5, SilencePolicy::strict))
        .process();

    // TODO: Test correlation threshold once DSP chaining is introduced
}

HART_TEST ("CorrelationAbove")
{
    const double defaultRenderDuration = hart::CLIConfig::getInstance().getDefaultRenderDurationSeconds();
    const unsigned int randomSeed = static_cast<unsigned int> (hart::CLIConfig::getInstance().getRandomSeed());
    std::srand (randomSeed);

    processAudioWith (GainDb (0_dB))
        .withLabel ("Identical signal")
        .withInputSignal (SineWave())
        .expectTrue (CorrelationAbove (0.999))
        .process();

    processAudioWith (GainDb (-6_dB))
        .withLabel ("Level difference doesn't matter")
        .withInputSignal (SineWave())
        .expectTrue (CorrelationAbove (0.999))
        .process();

    processAudioWith (TimeShift (10_ms))
        .withLabel ("Time shift within expected lag is fine")
        .withInputSignal (SineSweep())
        .expectTrue (CorrelationAbove (0.999, 100_ms))
        .process();

    processAudioWith (TimeShift (10_ms))
        .withLabel ("Time shift beyond expected lag results in a failure")
        .withInputSignal (SineSweep())
        .expectFalse (CorrelationAbove (0.999, 5_ms))
        .process();

    processAudioWith (GainLinear (-1.0))
        .withLabel ("Flipped polarity does not matter")
        .withInputSignal (SineWave())
        .expectTrue (CorrelationAbove (0.999))
        .process();

    processAudioWith ([] (float x) { return x + 0.5f; })
        .withLabel ("DC offset matters")
        .withInputSignal (SineWave())
        .expectTrue (CorrelationAbove (0.8))
        .expectFalse (CorrelationAbove (0.85))
        .process();

    processAudioWith (HardClip (-6_dB))
        .withLabel ("Moderately distorted")
        .withInputSignal (SineWave())
        .expectTrue (CorrelationAbove (0.97))
        .expectFalse (CorrelationAbove (0.999))
        .process();

    processAudioWith (HardClip (-30_dB))
        .withLabel ("Heavily distorted")
        .withInputSignal (SineWave())
        .expectTrue (CorrelationAbove (0.9))
        .expectFalse (CorrelationAbove (0.95))
        .process();

    processAudioWith ([] (float x) { return 0.01f * x + (static_cast<float> (std::rand()) / static_cast<float> (RAND_MAX) * 2 - 1);})
        .withLabel ("Very noisy")
        .withInputSignal (SineWave())
        .expectTrue (CorrelationAbove (0.01))
        .expectFalse (CorrelationAbove (0.1))
        .process();

    processAudioWith ([] (float) { return static_cast<float> (std::rand()) / static_cast<float> (RAND_MAX); })
        .withLabel ("Completely uncorrelated")
        .withDuration (std::max (defaultRenderDuration, 100_ms))
        .withInputSignal (SineWave())
        .expectFalse (CorrelationAbove (0.05))
        .process();
}

HART_TEST ("PolarityPreserved")
{
    using SilencePolicy = hart::SilencePolicy;

    const double defaultRenderDuration = hart::CLIConfig::getInstance().getDefaultRenderDurationSeconds();
    const unsigned int randomSeed = static_cast<unsigned int> (hart::CLIConfig::getInstance().getRandomSeed());
    std::srand (randomSeed);

    processAudioWith (GainDb (0_dB))
        .withLabel ("Identical signal")
        .withInputSignal (SineSweep())
        .expectTrue (PolarityPreserved())
        .process();

    processAudioWith (TimeShift (10_ms))
        .withLabel ("Time shift within expected lag is fine")
        .withDuration (std::max (defaultRenderDuration, 100_ms))
        .withInputSignal (SineSweep())
        .expectTrue (PolarityPreserved (0.5, 100_ms))
        .process();

    processAudioWith (TimeShift (10_ms))
        .withLabel ("Time shift beyond expected lag results in a failure")
        .withDuration (std::max (defaultRenderDuration, 100_ms))
        .withInputSignal (SineSweep())
        .expectFalse (PolarityPreserved (0.5, 5_ms))
        .process();

    processAudioWith (GainLinear (-0.5))
        .withLabel ("Negative linear gain")
        .withInputSignal (SineSweep())
        .expectFalse (PolarityPreserved())
        .process();

    // Even if just one channel is flipped, the matcher should report a failure
    processAudioWith (GainLinear (-1.0).atChannel (hart::Channel::right))
        .withLabel ("One channel is in phase, other one is flipped")
        .withInputSignal (SineSweep())
        .inStereo()
        .expectFalse (PolarityPreserved())
        .process();

    // Heavy clipping brings correlation down to 0.9 ish
    processAudioWith (HardClip (-40_dB))
        .withLabel ("Correlation thresholds - Distortion")
        .withDuration (std::max (defaultRenderDuration, 50_ms))
        .withInputSignal (SineSweep())
        .expectTrue (PolarityPreserved (0.5))  // Relaxed threshold
        .expectFalse (PolarityPreserved (0.95))  // Pretty strict threshold
        .process();

    auto additiveNoise = DSPFunction (
        [] (hart::AudioBuffer<float>& buffer) {
            for (size_t channel = 0; channel < buffer.getNumChannels(); ++channel)
                for (size_t i = 0; i < buffer.getNumFrames(); ++i)
                    buffer[channel][i] = 0.75f * buffer[channel][i] + (std::rand() / RAND_MAX * 2 - 1);
        },
        "Additive noise"
        );

    // Additive noise brings correlation down to 0.4 ish
    processAudioWith (std::move (additiveNoise))
        .withLabel ("Correlation thresholds - Additive noise")
        .withDuration (std::max (defaultRenderDuration, 100_ms))
        .withInputSignal (SineSweep())
        .expectTrue (PolarityPreserved (0.4))  // Relaxed threshold
        .expectFalse (PolarityPreserved (0.9))  // Pretty strict threshold
        .process();

    processAudioWith (GainLinear (0.0).atChannel (1))
        .withLabel ("Silence policies - One of the channels is muted")
        .withDuration (std::max (defaultRenderDuration, 50_ms))
        .withInputSignal (SineSweep())
        .inStereo()
        .expectTrue (PolarityPreserved (0.5, 10_ms, SilencePolicy::relaxed))
        .expectFalse (PolarityPreserved (0.5, 10_ms, SilencePolicy::strict))
        .process();

    auto drownRightChannelInNoise = DSPFunction (
        [] (hart::AudioBuffer<float>& buffer) {
            float* rightChannelSamples = buffer[hart::Channel::right];

            for (size_t i = 0; i < buffer.getNumFrames(); ++i)
                rightChannelSamples[i] = 0.001f * rightChannelSamples[i] + (std::rand() / RAND_MAX * 2 - 1);
        },
        "Drown right channel in noise"
        );

    // Very low correlation results in a failure, if it's not caused by silence
    processAudioWith (std::move (drownRightChannelInNoise))
        .withLabel ("Silence policies - One of the channels is weakly correlated, but not silent")
        .withDuration (std::max (defaultRenderDuration, 11_ms))
        .withInputSignal (SineSweep())
        .inStereo()
        .expectFalse (PolarityPreserved (0.5, 10_ms, SilencePolicy::relaxed))
        .expectFalse (PolarityPreserved (0.5, 10_ms, SilencePolicy::strict))
        .process();

    // If all channels are silent, the matcher should report a failure in any policy setting
    processAudioWith (GainLinear (0.0))
        .withLabel ("Silence policies - All the output channels are silent")
        .withDuration (std::max (defaultRenderDuration, 11_ms))
        .withInputSignal (SineSweep())
        .inStereo()
        .expectFalse (PolarityPreserved (0.5, 10_ms, SilencePolicy::relaxed))
        .expectFalse (PolarityPreserved (0.5, 10_ms, SilencePolicy::strict))
        .process();
}

HART_TEST ("TruePeaksBelow")
{
    using Oversampling = hart::Oversampling;
    using FilterQuality = TruePeaksBelow::FilterQuality;
    using Strictness = TruePeaksBelow::Strictness;

    processAudioWith (GainDb (0_dB))
        .withLabel ("Regular Sine wave")
        .withInputSignal (SineWave())
        .expectTrue (PeaksAt (0_dB))
        .expectTrue (TruePeaksBelow (0_dB))
        .expectTrue (TruePeaksBelow (0_dB, Oversampling::x8, FilterQuality::medium))
        .expectFalse (TruePeaksBelow (0_dB, Oversampling::x8, FilterQuality::medium, Strictness::strict))
        .expectFalse (TruePeaksBelow (0.01_dB, Oversampling::x8, FilterQuality::medium, Strictness::strict))
        .process();

    processAudioWith (HART_DSP_SEQUENCE (GainDb (12_dB) >> HardClip (0_dB)))
        .withLabel ("Slammed Sine wave")
        .withInputSignal (SineWave())
        .expectTrue (PeaksAt (0_dB))
        .expectFalse (TruePeaksBelow (0_dB))
        .expectTrue (TruePeaksBelow (1_dB))
        .expectTrue (TruePeaksBelow (2_dB, Oversampling::x8, FilterQuality::medium))
        .expectTrue (TruePeaksBelow (2_dB, Oversampling::x8, FilterQuality::medium, Strictness::strict))
        .process();

    processAudioWith (GainDb (0_dB))
        .withLabel ("White Noise")
        .withInputSignal (WhiteNoise())
        .expectTrue (PeaksAt (0_dB))
        .expectFalse (TruePeaksBelow (0_dB))
        .expectFalse (TruePeaksBelow (3_dB))
        .expectTrue (TruePeaksBelow (4_dB))
        .process();

    auto quarterSampleRateSignal = SignalFunction (
        [] (hart::AudioBuffer<float>& buffer)
        {
            buffer.setNumFrames (4);

            for (size_t channel = 0; channel < buffer.getNumChannels(); ++channel)
            {
                float* channelData = buffer[channel];
                channelData[0] = 0.0f;
                channelData[1] = 1.0f;
                channelData[2] = 0.0f;
                channelData[3] = -1.0f;
            }
        },
        "SR/4 Signal",
        hart::Loop::yes
        );

    processAudioWith (GainDb (0_dB))
        .withLabel ("SR/4 Signal")
        .withInputSignal (std::move (quarterSampleRateSignal))
        .expectTrue (PeaksAt (0_dB))
        .expectFalse (TruePeaksBelow (0_dB, Oversampling::x8, FilterQuality::medium))
        .expectTrue (TruePeaksBelow (0.2_dB, Oversampling::x8, FilterQuality::medium))
        .expectFalse (TruePeaksBelow (0.2_dB, Oversampling::x8, FilterQuality::medium, Strictness::strict))
        .expectTrue (TruePeaksBelow (0.3_dB, Oversampling::x8, FilterQuality::medium, Strictness::strict))
        .process();

    auto nyquistSignal = SignalFunction (
        [] (hart::AudioBuffer<float>& buffer)
        {
            buffer.setNumFrames (2);

            for (size_t channel = 0; channel < buffer.getNumChannels(); ++channel)
            {
                float* channelData = buffer[channel];
                channelData[0] = 1.0f;
                channelData[1] = -1.0f;
            }
        },
        "Nyquist Signal",
        hart::Loop::yes
        );

    processAudioWith (GainDb (0_dB))
        .withLabel ("Nyquist signal")
        .withInputSignal (std::move (nyquistSignal))
        .expectTrue (PeaksAt (0_dB))
        .expectFalse (TruePeaksBelow (0_dB))
        .expectTrue (TruePeaksBelow (2.5_dB))
        .expectFalse (TruePeaksBelow (2.5_dB, Oversampling::x8, FilterQuality::medium, Strictness::strict))
        .process();
}
