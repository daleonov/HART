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

HART_TEST ("LatencyBelow")
{
    processAudioWith (GainDb (0_dB))
        .withLabel ("No latency")
        .withInputSignal (SineWave())
        .expectTrue (LatencyBelow (100_us))
        .process();

    processAudioWith (TimeShift (5_ms))
        .withLabel ("Impulse into TimeShift - mono")
        .withInputSignal (Impulse())
        .expectTrue (LatencyBelow (5.1_ms))
        .expectFalse (LatencyBelow (4.9_ms))
        .process();

    processAudioWith (TimeShift (5_ms))
        .withLabel ("Impulse into TimeShift - 5 channels")
        .withInputChannels (5)
        .withOutputChannels (5)
        .withInputSignal (Impulse())
        .expectTrue (LatencyBelow (5.1_ms))
        .expectFalse (LatencyBelow (4.9_ms))
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
        .expectTrue (LatencyBelow (5.1_ms))
        .expectFalse (LatencyBelow (4.9_ms))
        .process();
}
