#include <cmath>  // abs(), isnan()

#include "hart.hpp"

HART_DECLARE_ALIASES_FOR_FLOAT;
using AudioBuffer = hart::AudioBuffer<float>;
using hart::floatsEqual;

HART_TEST ("Metrics - Channel Correlation")
{
    using hart::channelCorrelation;
    using hart::Channel;

    processAudioWith (GainDb (0_dB))
        .withLabel ("Identical channels")
        .withInputSignal (SineSweep())
        .inStereo()
        .expectTrue ([] (const AudioBuffer& output) { return HART_GT (channelCorrelation (output), 0.999); }, "channelCorrelation() - Left vs Right")
        .process();

    processAudioWith (GainDb (0_dB))
        .withLabel ("No correlation")
        .withInputSignal (WhiteNoise())
        .inStereo()
        .expectTrue ([] (const AudioBuffer& output) { return HART_LT (std::abs (channelCorrelation (output)), 0.1); }, "channelCorrelation() - Left vs Right")
        .process();

    processAudioWith (GainLinear (-1.0).atChannel (Channel::left))
        .withLabel ("Channels out of phase")
        .withInputSignal (SineSweep())
        .inStereo()
        .expectTrue ([] (const AudioBuffer& output) { return HART_LT (channelCorrelation (output), -0.999); }, "channelCorrelation() - Left vs Right").process();

    processAudioWith (Mute().atChannel (Channel::left))
        .withLabel ("Muted channel")
        .withInputSignal (SineSweep())
        .inStereo()
        .expectTrue ([] (const AudioBuffer& output) { return HART_TRUE (std::isnan (channelCorrelation (output))); }, "channelCorrelation() with default channels")
        .process();

    auto multiChannelSignal = SineSweep()
        >> GainLinear (-1.0).atChannel (1)
        >> GainDb (-30_dB).atChannel (2) >> AdditiveNoise (-3_dB).atChannel (2);

    processAudioWith (GainDb (0_dB))
        .withLabel ("Multi-channel")
        .withInputChannels (4)
        .withOutputChannels (4)
        .withInputSignal (std::move (multiChannelSignal))
        .expectTrue ([] (const AudioBuffer& output) { return HART_FLOAT_EQ (channelCorrelation (output, 2, 1), channelCorrelation (output, 1, 2), 1e-8); }, "channelCorrelation() - Channel order does not matter - 1 vs 2")
        .expectTrue ([] (const AudioBuffer& output) { return HART_FLOAT_EQ (channelCorrelation (output, 1, 0), channelCorrelation (output, 0, 1), 1e-8); }, "channelCorrelation() - Channel order does not matter - 0 vs 1")
        .expectTrue ([] (const AudioBuffer& output) { return HART_LT (channelCorrelation (output, 0, 1), -0.999); }, "channelCorrelation() Channels 0 vs 1")
        .expectTrue ([] (const AudioBuffer& output) { return HART_LT (std::abs (channelCorrelation (output, 0, 2)), 0.5); }, "channelCorrelation() - Channels 0 vs 2")
        .expectTrue ([] (const AudioBuffer& output) { return HART_LT (std::abs (channelCorrelation (output, 2, 1)), 0.5); }, "channelCorrelation() - Channels 2 vs 1")
        .expectTrue ([] (const AudioBuffer& output) { return HART_GT (channelCorrelation (output, 0, 3), 0.999); }, "channelCorrelation() - Channels 0 vs 3")
        .process();
}

HART_TEST ("Crest factor")
{
    using AudioBuffer = hart::AudioBuffer<float>;
    using Channel = hart::Channel;
    using hart::floatsEqual;
    using hart::crestFactorLinear;
    using hart::crestFactorDb;
    using std::sqrt;

    processAudioWith (GainDb (0_dB))
        .withLabel ("Crest factor of a sine wave")
        .withInputSignal (SineWave())
        .inMono()
        .expectTrue ([] (const AudioBuffer& output) { return HART_FLOAT_EQ (crestFactorLinear (output), sqrt (2.0), 1e-3); }, "Linear crest factor is around sqrt (2)")
        .expectTrue ([] (const AudioBuffer& output) { return HART_FLOAT_EQ (crestFactorLinear (output, 0), sqrt (2.0), 1e-3); }, "Linear crest factor is around sqrt (2)")
        .expectTrue ([] (const AudioBuffer& output) { return HART_FLOAT_EQ (crestFactorLinear (output, Channel::left), sqrt (2.0), 1e-3); }, "Linear crest factor is around sqrt (2)")
        .expectTrue ([] (const AudioBuffer& output) { return HART_FLOAT_EQ (crestFactorDb (output), 3.01_dB, 0.01); }, "Crest factor is around 3.01 dB")
        .expectTrue ([] (const AudioBuffer& output) { return HART_FLOAT_EQ (crestFactorDb (output, 0), 3.01_dB, 0.01); }, "Crest factor is around 3.01 dB")
        .expectTrue ([] (const AudioBuffer& output) { return HART_FLOAT_EQ (crestFactorDb (output, Channel::left), 3.01_dB, 0.01); }, "Crest factor is around 3.01 dB")
        .process();

    hart::DSPFunction<float> halfWaveRectify (
        [] (float x) { return x < 0.0f ? 0.0f : x; },
        "Half Wave Rectifier"
        );

    processAudioWith (GainDb (0_dB))
        .withLabel ("Crest factor of a half-rectified sine wave")
        .withInputSignal (SineWave() >> std::move (halfWaveRectify))
        .inMono()
        .expectTrue ([] (const AudioBuffer& output) { return HART_FLOAT_EQ (crestFactorLinear (output), 2.0, 1e-3); }, "Linear crest factor is around 2")
        .expectTrue ([] (const AudioBuffer& output) { return HART_FLOAT_EQ (crestFactorDb (output), 6.02_dB, 0.01); }, "Crest factor is around 6.02 dB")
        .process();

    processAudioWith (GainDb (0_dB))
        .withLabel ("Crest factor of an impulse")
        .withInputSignal (Impulse())
        .inMono()
        .expectTrue ([] (const AudioBuffer& output) { return HART_GT (crestFactorLinear (output), 20.0); }, "Linear crest factor is more than 20")
        .expectTrue ([] (const AudioBuffer& output) { return HART_GT (crestFactorDb (output), 10_dB); }, "Crest factor is around 10 dB")
        .process();

    const auto sharpTransientEnvelope = SegmentedEnvelope (0_dB)
        .hold (1_ms)
        .rampTo (-60_dB, 5_ms);

    processAudioWith (GainDb (0_dB))
        .withLabel ("Crest factor of a poky sine wave")
        .withInputSignal (SineWave() >> GainDb().withEnvelope (GainDb::gainDb, sharpTransientEnvelope))
        .inMono()
        .expectTrue ([] (const AudioBuffer& output) { return HART_GT (crestFactorLinear (output), 4.0); }, "Linear crest factor is more than 4")
        .expectTrue ([] (const AudioBuffer& output) { return HART_GT (crestFactorDb (output), 12_dB); }, "Crest factor is more than 12 dB")
        .process();

    for (double signalLevelDb : { -60_dB, -3_dB, -0.5_dB, +1_dB, +12_dB })
    {
        processAudioWith (GainDb (0_dB))
            .withLabel (HART_STR ("Gain doesn't matter, input level: " << hart::dbPrecision << signalLevelDb << " dB"))
            .withInputSignal (SineWave() >> GainDb (signalLevelDb))
            .inMono()
            .expectTrue ([] (const AudioBuffer& output) { return HART_FLOAT_EQ (crestFactorLinear (output), sqrt (2.0), 1e-3); }, "Linear crest factor is around sqrt (2)")
            .expectTrue ([] (const AudioBuffer& output) { return HART_FLOAT_EQ (crestFactorDb (output, 0), 3.01_dB, 0.01); }, "Crest factor is around 3.01 dB")
            .process();
    }

    processAudioWith (GainDb (0_dB))
        .withLabel ("Multi-channel")
        .withInputSignal (SineWave() >> GainDb().withEnvelope (GainDb::gainDb, sharpTransientEnvelope).atChannels ({1, 3}))
        .withInputChannels (5)
        .withOutputChannels (5)
        .expectTrue ([] (const AudioBuffer& output) { return HART_FLOAT_EQ (crestFactorLinear (hart::first(), output, {0, 2, 4}), sqrt (2.0), 1e-3); }, "Linear crest factor on steady channels is around sqrt (2)")
        .expectTrue ([] (const AudioBuffer& output) { return HART_TRUE (crestFactorLinear (hart::allFloatsEqualToEachOther(), output, {0, 2, 4})); }, "Linear crest factor on steady channels are the same")
        .expectTrue ([] (const AudioBuffer& output) { return HART_FLOAT_EQ (crestFactorDb (hart::first(), output, {0, 2, 4}), 3.01_dB, 0.01); }, "Crest factor on steady channels is around 3.01 dB")
        .expectTrue ([] (const AudioBuffer& output) { return HART_TRUE (crestFactorDb (hart::allFloatsEqualToEachOther(), output, {0, 2, 4})); }, "Crest factors in dB on steady channels are the same")
        .expectTrue ([] (const AudioBuffer& output) { return HART_GT (crestFactorLinear (hart::first(), output, {1, 3}), 4.0); }, "Linear crest factor on poky channels is more than 4")
        .expectTrue ([] (const AudioBuffer& output) { return HART_TRUE (crestFactorLinear (hart::allFloatsEqualToEachOther(), output, {1, 3})); }, "Linear crest factors on poky channels are the same")
        .expectTrue ([] (const AudioBuffer& output) { return HART_GT (crestFactorDb (hart::first(), output, {1, 3}), 12_dB); }, "Crest factor in dB on poky channels is over 12 dB")
        .expectTrue ([] (const AudioBuffer& output) { return HART_TRUE (crestFactorDb (hart::allFloatsEqualToEachOther(), output, {1, 3})); }, "Crest factors in dB on poky channels are the same")
        .process();
}

HART_TEST ("ESR")
{
    using AudioBuffer = hart::AudioBuffer<float>;
    using Channel = hart::Channel;
    using hart::esr;
    using std::isnan;

    processAudioWith (GainDb (0_dB))
        .withLabel ("Identical signal")
        .withInputSignal (SineSweep())
        .inMono()
        .expectTrue ([] (const AudioBuffer& input, const AudioBuffer& output) { return HART_FLOAT_EQ (esr (input, output), 0.0, 1e-8); }, "ESR ~= 0")
        .process();

    processAudioWith (GainDb (-3_dB))
        .withLabel ("Level mismatch matters")
        .withInputSignal (SineSweep())
        .inMono()
        .expectTrue ([] (const AudioBuffer& input, const AudioBuffer& output) { return HART_GT (esr (input, output), 0.01); }, "ESR > 0.01")
        .process();

    processAudioWith (AdditiveNoise (-20_dB))
        .withLabel ("A little bit of noise")
        .withInputSignal (SineSweep())
        .inMono()
        .expectTrue ([] (const AudioBuffer& input, const AudioBuffer& output) { return HART_FLOAT_IN_RANGE (esr (input, output), 0.0, 0.01, 1e-8); }, "ESR < 0.01")
        .process();

    processAudioWith (AdditiveNoise (-6_dB))
        .withLabel ("Quite a bit of noise")
        .withInputSignal (SineSweep())
        .inMono()
        .expectTrue ([] (const AudioBuffer& input, const AudioBuffer& output) { return HART_FLOAT_IN_RANGE (esr (input, output), 0.1, 0.2, 1e-8); }, "0.1 <= ESR <= 0.2")
        .process();

    processAudioWith (HART_DSP_SEQUENCE (Mute() >> AdditiveNoise (-12_dB)))
        .withLabel ("Just noise")
        .withInputSignal (SineSweep())
        .inMono()
        .expectTrue ([] (const AudioBuffer& input, const AudioBuffer& output) { return HART_GREATER_THAN (esr (input, output), 1.0); }, "ESR > 1")
        .process();

    processAudioWith (AdditiveNoise (-12_dB))
        .withLabel ("Reference signal has zero energy")
        .withInputSignal (Silence())
        .inMono()
        .expectTrue ([] (const AudioBuffer& input, const AudioBuffer& output) { return HART_TRUE (isnan (esr (input, output))); }, "ESR is undefined")
        .process();

    processAudioWith (Mute())
        .withLabel ("Output signal has no energy")
        .withInputSignal (SineSweep())
        .inMono()
        .expectTrue ([] (const AudioBuffer& input, const AudioBuffer& output) { return HART_FLOAT_EQ (esr (input, output), 1.0, 1e-8); }, "ESR ~= 1")
        .process();

    processAudioWith (GainLinear (-1.0))
        .withLabel ("Output signal is perfectly out of phase")
        .withInputSignal (SineSweep())
        .inMono()
        .expectTrue ([] (const AudioBuffer& input, const AudioBuffer& output) { return HART_FLOAT_EQ (esr (input, output), 4.0, 1e-8); }, "ESR ~= 4")
        .process();

    processAudioWith (GainLinear (2.0))
        .withLabel ("Output signal is exactly 2x input")
        .withInputSignal (SineSweep())
        .inMono()
        .expectTrue ([] (const AudioBuffer& input, const AudioBuffer& output) { return HART_FLOAT_EQ (esr (input, output), 1.0, 1e-8); }, "ESR ~= 4")
        .process();

    processAudioWith (AdditiveNoise (-20_dB))
        .withLabel ("Non-commutativity for non-identical signals")
        .withInputSignal (SineSweep())
        .inMono()
        .expectTrue ([] (const AudioBuffer& input, const AudioBuffer& output) { return HART_FLOAT_NE (esr (input, output), esr (output, input), 1e-8); }, "ESR (x, y) != ESR (y, x)")
        .process();

    using hart::first;
    using hart::last;
    using hart::nth;

    processAudioWith (HART_DSP_SEQUENCE (GainLinear (1.0) >> GainLinear (-1.0).atChannel (1) >> AdditiveNoise (-20_dB).atChannel (2) >> AdditiveNoise (-6_dB).atChannel (3) >> GainDb (-3.0).atChannel (4)))
        .withLabel ("Multi-channel")
        .withInputSignal (SineSweep())
        .withInputChannels (5)
        .withOutputChannels (5)
        .expectTrue ([] (const AudioBuffer& input, const AudioBuffer& output) { return HART_FLOAT_EQ (esr (input, output, 0), 0.0, 1e-8); }, "ESR ~= 1 at channel 0")
        .expectTrue ([] (const AudioBuffer& input, const AudioBuffer& output) { return HART_FLOAT_EQ (esr (input, output, 1), 4.0, 1e-8); }, "ESR ~= 4 at channel 1")
        .expectTrue ([] (const AudioBuffer& input, const AudioBuffer& output) { return HART_FLOAT_IN_RANGE (esr (input, output, 2), 0.0, 0.01, 1e-8); }, "ESR < 0.01 at channel 2")
        .expectTrue ([] (const AudioBuffer& input, const AudioBuffer& output) { return HART_FLOAT_IN_RANGE (esr (input, output, 3), 0.1, 0.2, 1e-8); }, "0.1 <= ESR <= 0.2 at channel 3")
        .expectTrue ([] (const AudioBuffer& input, const AudioBuffer& output) { return HART_GT (esr (input, output, 4), 0.01); }, "ESR > 0.01 at channel 4")

        .expectTrue ([] (const AudioBuffer& input, const AudioBuffer& output) { return HART_FLOAT_EQ (esr (first(), input, output), esr (input, output, 0), 1e-8); }, "esr() multi- vs single-channel overload at channel 0")
        .expectTrue ([] (const AudioBuffer& input, const AudioBuffer& output) { return HART_FLOAT_EQ (esr (nth (1), input, output), esr (input, output, 1), 1e-8); }, "esr() multi- vs single-channel overload at channel 1")
        .expectTrue ([] (const AudioBuffer& input, const AudioBuffer& output) { return HART_FLOAT_EQ (esr (nth (2), input, output), esr (input, output, 2), 1e-8); }, "esr() multi- vs single-channel overload at channel 2")
        .expectTrue ([] (const AudioBuffer& input, const AudioBuffer& output) { return HART_FLOAT_EQ (esr (nth (3), input, output), esr (input, output, 3), 1e-8); }, "esr() multi- vs single-channel overload at channel 3")
        .expectTrue ([] (const AudioBuffer& input, const AudioBuffer& output) { return HART_FLOAT_EQ (esr (last(),  input, output), esr (input, output, 4), 1e-8); }, "esr() multi- vs single-channel overload at channel 4")
        .expectTrue ([] (const AudioBuffer& input, const AudioBuffer& output) { return HART_FLOAT_EQ (esr (nth (3), input, output), esr (nth (1), input, output, {4, 3, 1}), 1e-8); }, "esr() with custom channel subset A")
        .expectTrue ([] (const AudioBuffer& input, const AudioBuffer& output) { return HART_FLOAT_EQ (esr (first(), input, output), esr (nth (2), input, output, {2, 4, 0, 1}), 1e-8); }, "esr() with custom channel subset B")

        .process();
}
