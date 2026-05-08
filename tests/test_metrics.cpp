#include <cmath>  // abs(), isnan()

#include "hart.hpp"

HART_DECLARE_ALIASES_FOR_FLOAT;
HART_DECLARE_ALIASES_FOR_UNITS;
using AudioBuffer = hart::AudioBuffer<float>;
using hart::floatsEqual;

HART_TEST ("MetricQuery and Sample Peak")
{
    // This test case is mostly for checking MetricQuery mechanics, samplePeak is merely a guinea pig here

    using hart::samplePeak;
    using hart::first;
    using hart::last;
    using hart::max;
    using hart::mean;
    using hart::nth;
    using hart::size;
    using hart::decibelsToRatio;
    using hart::ratioToDecibels;

    for (const size_t numChannels : { 1, 2, 5, 15 })
    {
        processAudioWith (GainDb (0_dB))
            .withLabel (HART_STR ("Output vector size at " << numChannels << " channels"))
            .withInputSignal (SineWave())
            .withInputChannels (numChannels)
            .withOutputChannels (numChannels)
            .withDuration (1_ms)
            .expectTrue ([numChannels] (const AudioBuffer& output) { return HART_EQ (samplePeak (output).get (size()), numChannels); }, "Correct vector size")
            .process();
    }

    for (const double levelDb : {-3_dB, 0_dB, -12.34_dB, 0.1_dB})
    {
        const double levelLinear = decibelsToRatio (levelDb);

        processAudioWith (GainDb (0_dB))
            .withLabel (HART_STR ("Mono, level at " << levelDb << " dB"))
            .withInputSignal (SineWave() >> GainDb (levelDb))
            .inMono()
            .expectTrue ([levelDb] (const AudioBuffer& output) { return HART_FLOAT_EQ ((double) samplePeak (output).as (dB), levelDb, 1e-2); }, "Sample Peak in dB, C-style cast")
            .expectTrue ([levelDb] (const AudioBuffer& output) { return HART_FLOAT_EQ (samplePeak (output).as (dB).get(), levelDb, 1e-2); }, "Sample Peak in dB, default getter")
            .expectTrue ([levelDb] (const AudioBuffer& output) { return HART_FLOAT_EQ (samplePeak (output).as (dB).get (first()), levelDb, 1e-2); }, "Sample Peak in dB, first() reducer")
            .expectTrue ([levelLinear] (const AudioBuffer& output) { return HART_FLOAT_EQ (samplePeak (output).as (linear).get(), levelLinear, 1e-3); }, "Sample Peak as linear value")
            .expectTrue ([levelLinear] (const AudioBuffer& output) { return HART_FLOAT_EQ (samplePeak (output).as (native).get(), levelLinear, 1e-3); }, "Sample Peak as native value, same as linear")
            .process();
    }

    const double expectedDbMean = (-3_dB + -6_dB + -12_dB) / 3;
    const double expectedLinToDbMean = ratioToDecibels ((decibelsToRatio (-3_dB) + decibelsToRatio (-6_dB) + decibelsToRatio (-12_dB)) / 3);
    HART_ASSERT_FLOAT_NE (expectedDbMean, expectedLinToDbMean, 1e-8) << "Geometric vs arithmetic averaging";

    processAudioWith (GainDb (0_dB))
        .withLabel ("Multiple channels")
        .withInputSignal (SineWave() >> GainDb (-3_dB).atChannel (0) >> GainDb (-6_dB).atChannel (1) >> GainDb (-12_dB).atChannel (2))
        .withInputChannels (3)
        .withOutputChannels (3)

        .expectTrue ([] (const AudioBuffer& output) { return HART_EQ (samplePeak (output).get (size()), 3); }, "Correct vector size")

        .expectTrue ([] (const AudioBuffer& output) { return HART_FLOAT_EQ (samplePeak (output).as (dB).get (nth (0)), -3_dB, 1e-2); }, "Sample Peak in dB, n = 0")
        .expectTrue ([] (const AudioBuffer& output) { return HART_FLOAT_EQ (samplePeak (output).as (dB).get (nth (1)), -6_dB, 1e-2); }, "Sample Peak in dB, n = 1")
        .expectTrue ([] (const AudioBuffer& output) { return HART_FLOAT_EQ (samplePeak (output).as (dB).get (nth (2)), -12_dB, 1e-2); }, "Sample Peak in dB, n = 2")
            
        .expectTrue ([] (const AudioBuffer& output) { return HART_FLOAT_EQ (samplePeak (output).as (linear).get (nth (0)), decibelsToRatio (-3_dB), 1e-3); }, "Sample Peak linear, n = 0")
        .expectTrue ([] (const AudioBuffer& output) { return HART_FLOAT_EQ (samplePeak (output).as (linear).get (nth (1)), decibelsToRatio (-6_dB), 1e-3); }, "Sample Peak linear, n = 1")
        .expectTrue ([] (const AudioBuffer& output) { return HART_FLOAT_EQ (samplePeak (output).as (linear).get (nth (2)), decibelsToRatio (-12_dB), 1e-3); }, "Sample Peak linear, n = 2")
            
        .expectTrue ([] (const AudioBuffer& output) { return HART_FLOAT_EQ ((double) samplePeak (output).as (dB), -3_dB, 1e-2); }, "Sample Peak in dB, C-style cast")  // Not using a reducer isn't too useful for multi-channel metrics, but checking for test sake
        .expectTrue ([] (const AudioBuffer& output) { return HART_FLOAT_EQ (samplePeak (output).as (dB).get(), -3_dB, 1e-2); }, "Sample Peak in dB, default getter")  // Not using a reducer isn't too useful for multi-channel metrics, but checking for test sake

        .expectTrue ([] (const AudioBuffer& output) { return HART_FLOAT_EQ (samplePeak (output).as (dB).get (first()), -3_dB, 1e-2); }, "Sample Peak in dB, first() reducer")
        .expectTrue ([] (const AudioBuffer& output) { return HART_FLOAT_EQ (samplePeak (output).as (dB).get (last()), -12_dB, 1e-2); }, "Sample Peak in dB, last() reducer")

        .expectTrue ([expectedDbMean] (const AudioBuffer& output) { return HART_FLOAT_EQ (samplePeak (output).as (dB).get (mean()), expectedDbMean, 1e-2); }, "Sample Peak in dB, mean() reducer")
        .expectTrue ([expectedLinToDbMean] (const AudioBuffer& output) { return HART_FLOAT_EQ (ratioToDecibels (samplePeak (output).as (linear).get (mean())), expectedLinToDbMean, 1e-3); }, "Sample Peak linear, mean() reducer, then converted to dB")

        .process();

    // TODO: Test selected channels
    // TODO: Test slices
    // TODO: Test preserving index order for multi-channel selections
}

HART_TEST ("Metrics - Channel Correlation")
{
    using hart::channelCorrelation;
    using hart::Channel;

    processAudioWith (GainDb (0_dB))
        .withLabel ("Identical channels")
        .withInputSignal (SineSweep())
        .inStereo()
        .expectTrue ([] (const AudioBuffer& output) { return HART_GT (channelCorrelation (output).get(), 0.999); }, "channelCorrelation() - Left vs Right")
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
        .expectTrue ([] (const AudioBuffer& output) { return HART_LT (channelCorrelation (output).get(), -0.999); }, "channelCorrelation() - Left vs Right").process();

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
        .expectTrue ([] (const AudioBuffer& output) { return HART_FLOAT_EQ (channelCorrelation (output).ch ({{2, 1}}).get(), channelCorrelation (output).ch ({{1, 2}}).get(), 1e-8); }, "channelCorrelation() - Channel order does not matter - 1 vs 2")
        .expectTrue ([] (const AudioBuffer& output) { return HART_FLOAT_EQ (channelCorrelation (output).ch ({{1, 0}}).get(), channelCorrelation (output).ch ({{0, 1}}).get(), 1e-8); }, "channelCorrelation() - Channel order does not matter - 0 vs 1")
        .expectTrue ([] (const AudioBuffer& output) { return HART_LT (channelCorrelation (output).ch ({{0, 1}}).get(), -0.999); }, "channelCorrelation() Channels 0 vs 1")
        .expectTrue ([] (const AudioBuffer& output) { return HART_LT (std::abs (channelCorrelation (output).ch ({{0, 2}})), 0.5); }, "channelCorrelation() - Channels 0 vs 2")
        .expectTrue ([] (const AudioBuffer& output) { return HART_LT (std::abs (channelCorrelation (output).ch ({{2, 1}})), 0.5); }, "channelCorrelation() - Channels 2 vs 1")
        .expectTrue ([] (const AudioBuffer& output) { return HART_GT (channelCorrelation (output).ch ({{0, 3}}).get(), 0.999); }, "channelCorrelation() - Channels 0 vs 3")
        .process();
}

HART_TEST ("Crest factor")
{
    using AudioBuffer = hart::AudioBuffer<float>;
    using Channel = hart::Channel;
    using hart::floatsEqual;
    using hart::crestFactor;
    using std::sqrt;

    processAudioWith (GainDb (0_dB))
        .withLabel ("Crest factor of a sine wave")
        .withInputSignal (SineWave())
        .inMono()
        .expectTrue ([] (const AudioBuffer& output) { return HART_FLOAT_EQ (crestFactor (output).get(), sqrt (2.0), 1e-3); }, "Linear crest factor is around sqrt (2)")
        .expectTrue ([] (const AudioBuffer& output) { return HART_FLOAT_EQ (crestFactor (output).ch ({0}).get(), sqrt (2.0), 1e-3); }, "Linear crest factor is around sqrt (2)")
        .expectTrue ([] (const AudioBuffer& output) { return HART_FLOAT_EQ (crestFactor (output).ch ({Channel::left}).get(), sqrt (2.0), 1e-3); }, "Linear crest factor is around sqrt (2)")
        .expectTrue ([] (const AudioBuffer& output) { return HART_FLOAT_EQ (crestFactor (output). ch({0}).as (dB).get(), 3.01_dB, 0.01); }, "Crest factor is around 3.01 dB")
        .expectTrue ([] (const AudioBuffer& output) { return HART_FLOAT_EQ (crestFactor (output). ch({Channel::left}).as (dB).get(), 3.01_dB, 0.01); }, "Crest factor is around 3.01 dB")
        .process();

    processAudioWith (GainDb (0_dB))
        .withLabel ("Crest factor units")
        .withInputSignal (SineWave())
        .inMono()
        .expectTrue ([] (const AudioBuffer& output) { return HART_FLOAT_EQ (crestFactor (output).get(), crestFactor (output).as (native).get(), 1e-8); }, "default == native")
        .expectTrue ([] (const AudioBuffer& output) { return HART_FLOAT_EQ (crestFactor (output).as (linear).get(), crestFactor (output).as (native).get(), 1e-8); }, "linear == native")
        .expectTrue ([] (const AudioBuffer& output) { return HART_FLOAT_NE (crestFactor (output).as (linear).get(), crestFactor (output).as (dB).get(), 1e-8); }, "linear != dB")
        .process();

    hart::DSPFunction<float> halfWaveRectify (
        [] (float x) { return x < 0.0f ? 0.0f : x; },
        "Half Wave Rectifier"
        );

    processAudioWith (GainDb (0_dB))
        .withLabel ("Crest factor of a half-rectified sine wave")
        .withInputSignal (SineWave() >> std::move (halfWaveRectify))
        .inMono()
        .expectTrue ([] (const AudioBuffer& output) { return HART_FLOAT_EQ (crestFactor (output).get(), 2.0, 1e-3); }, "Linear crest factor is around 2")
        .expectTrue ([] (const AudioBuffer& output) { return HART_FLOAT_EQ (crestFactor (output).as (dB).get(), 6.02_dB, 0.01); }, "Crest factor is around 6.02 dB")
        .process();

    processAudioWith (GainDb (0_dB))
        .withLabel ("Crest factor of an impulse")
        .withInputSignal (Impulse())
        .inMono()
        .expectTrue ([] (const AudioBuffer& output) { return HART_GT (crestFactor (output).get(), 20.0); }, "Linear crest factor is more than 20")
        .expectTrue ([] (const AudioBuffer& output) { return HART_GT (crestFactor (output).as (dB).get(), 10_dB); }, "Crest factor is around 10 dB")
        .process();

    const auto sharpTransientEnvelope = SegmentedEnvelope (0_dB)
        .hold (1_ms)
        .rampTo (-60_dB, 5_ms);

    processAudioWith (GainDb (0_dB))
        .withLabel ("Crest factor of a poky sine wave")
        .withInputSignal (SineWave() >> GainDb().withEnvelope (GainDb::gainDb, sharpTransientEnvelope))
        .inMono()
        .expectTrue ([] (const AudioBuffer& output) { return HART_GT (crestFactor (output).get(), 4.0); }, "Linear crest factor is more than 4")
        .expectTrue ([] (const AudioBuffer& output) { return HART_GT (crestFactor (output).as (dB).get(), 12_dB); }, "Crest factor is more than 12 dB")
        .process();

    for (double signalLevelDb : { -60_dB, -3_dB, -0.5_dB, +1_dB, +12_dB })
    {
        processAudioWith (GainDb (0_dB))
            .withLabel (HART_STR ("Gain doesn't matter, input level: " << hart::dbPrecision << signalLevelDb << " dB"))
            .withInputSignal (SineWave() >> GainDb (signalLevelDb))
            .inMono()
            .expectTrue ([] (const AudioBuffer& output) { return HART_FLOAT_EQ (crestFactor (output).get(), sqrt (2.0), 1e-3); }, "Linear crest factor is around sqrt (2)")
            .expectTrue ([] (const AudioBuffer& output) { return HART_FLOAT_EQ (crestFactor (output).as (dB).get(), 3.01_dB, 0.01); }, "Crest factor is around 3.01 dB")
            .process();
    }

    using hart::first;
    using hart::allFloatsEqualToEachOther;

    processAudioWith (GainDb (0_dB))
        .withLabel ("Multi-channel")
        .withInputSignal (SineWave() >> GainDb().withEnvelope (GainDb::gainDb, sharpTransientEnvelope).atChannels ({1, 3}))
        .withInputChannels (5)
        .withOutputChannels (5)
        .expectTrue ([] (const AudioBuffer& output) { return HART_FLOAT_EQ (crestFactor (output).ch ({0, 2, 4}).get (first()), sqrt (2.0), 1e-3); }, "Linear crest factor on steady channels is around sqrt (2)")
        .expectTrue ([] (const AudioBuffer& output) { return HART_TRUE (crestFactor (output).ch ({0, 2, 4}).get (allFloatsEqualToEachOther())); }, "Linear crest factor on steady channels are the same")
        .expectTrue ([] (const AudioBuffer& output) { return HART_FLOAT_EQ (crestFactor (output).ch ({0, 2, 4}).as (dB).get (first()), 3.01_dB, 0.01); }, "Crest factor on steady channels is around 3.01 dB")
        .expectTrue ([] (const AudioBuffer& output) { return HART_TRUE (crestFactor (output).as (dB).ch ({0, 2, 4}).get (allFloatsEqualToEachOther())); }, "Crest factors in dB on steady channels are the same")
        .expectTrue ([] (const AudioBuffer& output) { return HART_GT (crestFactor (output).ch ({1, 3}).get (first()), 4.0); }, "Linear crest factor on poky channels is more than 4")
        .expectTrue ([] (const AudioBuffer& output) { return HART_TRUE (crestFactor (output).ch ({1, 3}).get (allFloatsEqualToEachOther())); }, "Linear crest factors on poky channels are the same")
        .expectTrue ([] (const AudioBuffer& output) { return HART_GT (crestFactor (output).as (dB).ch ({1, 3}).get (first()), 12_dB); }, "Crest factor in dB on poky channels is over 12 dB")
        .expectTrue ([] (const AudioBuffer& output) { return HART_TRUE (crestFactor (output).as (dB).ch ({1, 3}).get (allFloatsEqualToEachOther())); }, "Crest factors in dB on poky channels are the same")
        .process();
}

HART_TEST ("ESR")
{
    using AudioBuffer = hart::AudioBuffer<float>;
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
