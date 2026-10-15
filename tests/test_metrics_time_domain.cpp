#include <algorithm>  // max()
#include <cmath>  // abs(), isnan(), isinf(), cos(), acos(), tan()

#include "hart.hpp"
#include "exponential_decay.hpp"
#include "unstable_decay.hpp"

HART_DECLARE_ALIASES_FOR_FLOAT;
HART_DECLARE_ALIASES_FOR_UNITS;
using AudioBuffer = hart::AudioBuffer<float>;
using hart::floatsEqual;

HART_TEST ("Metrics - MetricQuery and Sample Peak")
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

    constexpr double expectedDbMean = (-3_dB + -6_dB + -12_dB) / 3;
    const double expectedLinToDbMean = ratioToDecibels ((decibelsToRatio (-3_dB) + decibelsToRatio (-6_dB) + decibelsToRatio (-12_dB)) / 3);
    HART_ASSERT_FLOAT_NE (expectedDbMean, expectedLinToDbMean, 1e-8) << "Geometric vs arithmetic averaging";

    processAudioWith (GainDb (0_dB))
        .withLabel ("Multiple channels")
        .withInputSignal (SineWave() >> GainDb (-3_dB).atChannel (0) >> GainDb (-6_dB).atChannel (1) >> GainDb (-12_dB).atChannel (2))
        .withInputChannels (3)
        .withOutputChannels (3)

        .expectTrue ([] (const AudioBuffer& output) { return HART_EQ (samplePeak (output).get (size()), 3u); }, "Correct vector size")

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

        .expectTrue ([=] (const AudioBuffer& output) { return HART_FLOAT_EQ (samplePeak (output).as (dB).get (mean()), expectedDbMean, 1e-2); }, "Sample Peak in dB, mean() reducer")
        .expectTrue ([expectedLinToDbMean] (const AudioBuffer& output) { return HART_FLOAT_EQ (ratioToDecibels (samplePeak (output).as (linear).get (mean())), expectedLinToDbMean, 1e-3); }, "Sample Peak linear, mean() reducer, then converted to dB")

        .process();

    using Slice = hart::Slice;
    const auto gainCurve = SegmentedEnvelope (decibelsToRatio (-3_dB))
        .hold (10_ms)
        .rampTo (decibelsToRatio (-6_dB), 1_ms)
        .hold (10_ms)
        .rampTo (decibelsToRatio (-1_dB), 1_ms);

    using hart::roundToSizeT;
    const double sampleRateHz = hart::CLIConfig::getInstance().getDefaultSampleRateHz();
    auto toFrames = [sampleRateHz] (double timeStampSeconds) { return roundToSizeT (timeStampSeconds * sampleRateHz); };

    processAudioWith (GainLinear().withEnvelope (GainLinear::gainLinear, gainCurve))
        .withLabel ("Slices")
        .withInputSignal (SineWave())
        .inMono()

        .expectTrue ([] (const AudioBuffer& output) { return HART_FLOAT_EQ (samplePeak (output).as (dB).get(), -1_dB, 0.01); }, "No slice")
        .expectTrue ([] (const AudioBuffer& output) { return HART_FLOAT_EQ (samplePeak (output).as (dB).at (Slice::whole()).get(), -1_dB, 0.01); }, "Whole")

        .expectTrue ([] (const AudioBuffer& output) { return HART_FLOAT_EQ (samplePeak (output).as (dB).at (Slice::time (0_ms, 9_ms)).get(), -3_dB, 0.01); }, "Time - Section A")
        .expectTrue ([] (const AudioBuffer& output) { return HART_FLOAT_EQ (samplePeak (output).as (dB).at (Slice::time (12_ms, 19_ms)).get(), -6_dB, 0.01); }, "Time - Section B")
        .expectTrue ([] (const AudioBuffer& output) { return HART_FLOAT_EQ (samplePeak (output).as (dB).at (Slice::time (22_ms, 30_ms)).get(), -1_dB, 0.01); }, "Time - Section C")
        .expectTrue ([] (const AudioBuffer& output) { return HART_FLOAT_EQ (samplePeak (output).as (dB).at (Slice::time (0_ms, 19_ms)).get(), -3_dB, 0.01); }, "Time - Sections A-B")
        .expectTrue ([] (const AudioBuffer& output) { return HART_FLOAT_EQ (samplePeak (output).as (dB).at (Slice::time (12_ms, 30_ms)).get(), -1_dB, 0.01); }, "Time - Sections B-C")

        .expectTrue ([&toFrames] (const AudioBuffer& output) { return HART_FLOAT_EQ (samplePeak (output).as (dB).at (Slice::frames (toFrames (0_ms), toFrames (9_ms))).get(), -3_dB, 0.01); }, "Frames - Section A")
        .expectTrue ([&toFrames] (const AudioBuffer& output) { return HART_FLOAT_EQ (samplePeak (output).as (dB).at (Slice::frames (toFrames (12_ms), toFrames (19_ms))).get(), -6_dB, 0.01); }, "Frames - Section B")
        .expectTrue ([&toFrames] (const AudioBuffer& output) { return HART_FLOAT_EQ (samplePeak (output).as (dB).at (Slice::frames (toFrames (22_ms), toFrames (30_ms))).get(), -1_dB, 0.01); }, "Frames - Section C")
        .expectTrue ([&toFrames] (const AudioBuffer& output) { return HART_FLOAT_EQ (samplePeak (output).as (dB).at (Slice::frames (toFrames (0_ms), toFrames (19_ms))).get(), -3_dB, 0.01); }, "Frames - Sections A-B")
        .expectTrue ([&toFrames] (const AudioBuffer& output) { return HART_FLOAT_EQ (samplePeak (output).as (dB).at (Slice::frames (toFrames (12_ms), toFrames (30_ms))).get(), -1_dB, 0.01); }, "Frames - Sections B-C")

        .process();

    // TODO: Test selected channels
    // TODO: Test preserving index order for multi-channel selections
}

HART_TEST ("Metrics - MetricQuery::apply() modifies per-channel values before the reducer")
{
    const AudioBuffer silence (5, 1);
    hart::MetricQuery<double> fiveZeros = hart::samplePeak (silence);

    HART_ASSERT_TRUE (fiveZeros.get (hart::allFloatsEqualTo (0.0)));

    // Function pointers - templated STL functions
    HART_EXPECT_FLOAT_EQ (fiveZeros.apply (std::cos).get (hart::mean()), 1.0, 1e-8);
    HART_EXPECT_FLOAT_EQ (fiveZeros.apply (std::cos).get (hart::sum()), 5.0, 1e-8);
    HART_EXPECT_FLOAT_EQ (fiveZeros.apply (std::acos).get (hart::mean()), hart::halfPi, 1e-8);
    HART_EXPECT_FLOAT_EQ (fiveZeros.apply (std::acos).get (hart::sum()), 2.5 * hart::pi, 1e-8);

    // Functors - capturing lambdas
    const double y = 2.0;  // Not constexpr on purpose, so that it *has* to be captured in lambdas
    HART_EXPECT_FLOAT_EQ (fiveZeros.apply ([y] (double x) { return std::pow (y, x); }).get (hart::mean()), 1.0, 1e-8);
    HART_EXPECT_FLOAT_EQ (fiveZeros.apply ([y] (double x) { return std::pow (y, x); }).get (hart::sum()), 5.0, 1e-8);
    HART_EXPECT_FLOAT_EQ (fiveZeros.apply ([y] (double x) { return x + y; }).get (hart::mean()), y, 1e-8);
    HART_EXPECT_FLOAT_EQ (fiveZeros.apply ([y] (double x) { return x + y; }).get (hart::sum()), 5.0 * y, 1e-8);

    // Functors - non-capturing lambdas
    HART_EXPECT_FLOAT_EQ (fiveZeros.apply ([] (double x) { return std::pow (123.0, x); }).get (hart::mean()), 1.0, 1e-8);
    HART_EXPECT_FLOAT_EQ (fiveZeros.apply ([] (double x) { return std::pow (123.0, x); }).get (hart::sum()), 5.0, 1e-8);
    HART_EXPECT_FLOAT_EQ (fiveZeros.apply ([] (double x) { return x + 3.0; }).get (hart::mean()), 3.0, 1e-8);
    HART_EXPECT_FLOAT_EQ (fiveZeros.apply ([] (double x) { return x + 3.0; }).get (hart::sum()), 15.0, 1e-8);
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

HART_TEST ("Metrics - Crest factor")
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

HART_TEST ("Metrics - ESR")
{
    using AudioBuffer = hart::AudioBuffer<float>;
    using hart::esr;
    using std::isnan;

    processAudioWith (GainDb (0_dB))
        .withLabel ("Identical signal")
        .withInputSignal (SineSweep())
        .inMono()
        .expectTrue ([] (const AudioBuffer& input, const AudioBuffer& output) { return HART_FLOAT_EQ (esr (input, output).get(), 0.0, 1e-8); }, "ESR ~= 0")
        .process();

    processAudioWith (GainDb (-3_dB))
        .withLabel ("Level mismatch matters")
        .withInputSignal (SineSweep())
        .inMono()
        .expectTrue ([] (const AudioBuffer& input, const AudioBuffer& output) { return HART_GT (esr (input, output).get(), 0.01); }, "ESR > 0.01")
        .process();

    processAudioWith (AdditiveNoise (-20_dB))
        .withLabel ("A little bit of noise")
        .withInputSignal (SineSweep())
        .inMono()
        .expectTrue ([] (const AudioBuffer& input, const AudioBuffer& output) { return HART_FLOAT_IN_RANGE (esr (input, output).get(), 0.0, 0.01, 1e-8); }, "ESR < 0.01")
        .process();

    processAudioWith (AdditiveNoise (-6_dB))
        .withLabel ("Quite a bit of noise")
        .withInputSignal (SineSweep())
        .inMono()
        .expectTrue ([] (const AudioBuffer& input, const AudioBuffer& output) { return HART_FLOAT_IN_RANGE (esr (input, output).get(), 0.1, 0.2, 1e-8); }, "0.1 <= ESR <= 0.2")
        .process();

    processAudioWith (HART_DSP_SEQUENCE (Mute() >> AdditiveNoise (-12_dB)))
        .withLabel ("Just noise")
        .withInputSignal (SineSweep())
        .inMono()
        .expectTrue ([] (const AudioBuffer& input, const AudioBuffer& output) { return HART_GREATER_THAN (esr (input, output).get(), 1.0); }, "ESR > 1")
        .process();

    processAudioWith (AdditiveNoise (-12_dB))
        .withLabel ("Reference signal has zero energy")
        .withInputSignal (Silence())
        .inMono()
        .expectTrue ([] (const AudioBuffer& input, const AudioBuffer& output) { return HART_TRUE (isnan (esr (input, output).get())); }, "ESR is undefined")
        .process();

    processAudioWith (Mute())
        .withLabel ("Output signal has no energy")
        .withInputSignal (SineSweep())
        .inMono()
        .expectTrue ([] (const AudioBuffer& input, const AudioBuffer& output) { return HART_FLOAT_EQ (esr (input, output).get(), 1.0, 1e-8); }, "ESR ~= 1")
        .process();

    processAudioWith (GainLinear (-1.0))
        .withLabel ("Output signal is perfectly out of phase")
        .withInputSignal (SineSweep())
        .inMono()
        .expectTrue ([] (const AudioBuffer& input, const AudioBuffer& output) { return HART_FLOAT_EQ (esr (input, output).get(), 4.0, 1e-8); }, "ESR ~= 4")
        .process();

    processAudioWith (GainLinear (2.0))
        .withLabel ("Output signal is exactly 2x input")
        .withInputSignal (SineSweep())
        .inMono()
        .expectTrue ([] (const AudioBuffer& input, const AudioBuffer& output) { return HART_FLOAT_EQ (esr (input, output).get(), 1.0, 1e-8); }, "ESR ~= 4")
        .process();

    processAudioWith (AdditiveNoise (-20_dB))
        .withLabel ("Non-commutativity for non-identical signals")
        .withInputSignal (SineSweep())
        .inMono()
        .expectTrue ([] (const AudioBuffer& input, const AudioBuffer& output) { return HART_FLOAT_NE (esr (input, output).get(), esr (output, input).get(), 1e-8); }, "ESR (x, y) != ESR (y, x)")
        .process();

    using hart::first;
    using hart::last;
    using hart::nth;

    processAudioWith (HART_DSP_SEQUENCE (GainLinear (1.0) >> GainLinear (-1.0).atChannel (1) >> AdditiveNoise (-20_dB).atChannel (2) >> AdditiveNoise (-6_dB).atChannel (3) >> GainDb (-3.0).atChannel (4)))
        .withLabel ("Multi-channel")
        .withInputSignal (SineSweep())
        .withInputChannels (5)
        .withOutputChannels (5)
        .expectTrue ([] (const AudioBuffer& input, const AudioBuffer& output) { return HART_FLOAT_EQ (esr (input, output).ch (0).get(), 0.0, 1e-8); }, "ESR ~= 1 at channel 0")
        .expectTrue ([] (const AudioBuffer& input, const AudioBuffer& output) { return HART_FLOAT_EQ (esr (input, output).ch (1).get(), 4.0, 1e-8); }, "ESR ~= 4 at channel 1")
        .expectTrue ([] (const AudioBuffer& input, const AudioBuffer& output) { return HART_FLOAT_IN_RANGE (esr (input, output).ch (2).get(), 0.0, 0.01, 1e-8); }, "ESR < 0.01 at channel 2")
        .expectTrue ([] (const AudioBuffer& input, const AudioBuffer& output) { return HART_FLOAT_IN_RANGE (esr (input, output).ch (3).get(), 0.1, 0.2, 1e-8); }, "0.1 <= ESR <= 0.2 at channel 3")
        .expectTrue ([] (const AudioBuffer& input, const AudioBuffer& output) { return HART_GT (esr (input, output).ch (4).get(), 0.01); }, "ESR > 0.01 at channel 4")

        .expectTrue ([] (const AudioBuffer& input, const AudioBuffer& output) { return HART_FLOAT_EQ (esr (input, output).get (first()), esr (input, output).ch (0).get(), 1e-8); }, "esr() multi- vs single-channel overload at channel 0")
        .expectTrue ([] (const AudioBuffer& input, const AudioBuffer& output) { return HART_FLOAT_EQ (esr (input, output).get (nth (1)), esr (input, output).ch (1).get(), 1e-8); }, "esr() multi- vs single-channel overload at channel 1")
        .expectTrue ([] (const AudioBuffer& input, const AudioBuffer& output) { return HART_FLOAT_EQ (esr (input, output).get (nth (2)), esr (input, output).ch (2).get(), 1e-8); }, "esr() multi- vs single-channel overload at channel 2")
        .expectTrue ([] (const AudioBuffer& input, const AudioBuffer& output) { return HART_FLOAT_EQ (esr (input, output).get (nth (3)), esr (input, output).ch (3).get(), 1e-8); }, "esr() multi- vs single-channel overload at channel 3")
        .expectTrue ([] (const AudioBuffer& input, const AudioBuffer& output) { return HART_FLOAT_EQ (esr (input, output).get (last()), esr (input, output).ch (4).get(), 1e-8); }, "esr() multi- vs single-channel overload at channel 4")
        .expectTrue ([] (const AudioBuffer& input, const AudioBuffer& output) { return HART_FLOAT_EQ (esr (input, output).get (nth (3)), esr (input, output).ch ({4, 3, 1}).get (nth (1)), 1e-8); }, "esr() with custom channel subset A")
        .expectTrue ([] (const AudioBuffer& input, const AudioBuffer& output) { return HART_FLOAT_EQ (esr (input, output).get (first()), esr (input, output).ch ({2, 4, 0, 1}).get (nth (2)), 1e-8); }, "esr() with custom channel subset B")

        .process();
}

HART_TEST ("Metrics - Max Cross Correlation")
{
    using AudioBuffer = hart::AudioBuffer<float>;
    using hart::maxCrossCorrelation;
    using std::abs;

    processAudioWith (GainDb (0_dB))
        .withLabel ("Identical signal")
        .withInputSignal (SineSweep())
        .expectTrue (
            [] (const AudioBuffer& input, const AudioBuffer& output)
            {
                return HART_FLOAT_EQ (maxCrossCorrelation (input, output, 10_ms).get(), 1.0, 1e-3);
            },
            "Perfectly correlated"
        )
        .process();

    processAudioWith (AdditiveNoise (-12_dB))
        .withLabel ("Moderately noisy")
        .withInputSignal (SineSweep() >> GainDb (-3_dB))
        .expectTrue (
            [] (const AudioBuffer& input, const AudioBuffer& output)
            {
                return HART_LT (maxCrossCorrelation (input, output, 10_ms).get(), 0.99);
            },
            "Moderately correlated"
        )
        .process();

    processAudioWith (AdditiveNoise (-12_dB))
        .withLabel ("Very noisy")
        .withInputSignal (SineSweep() >> GainDb (-12_dB))
        .expectTrue (
            [] (const AudioBuffer& input, const AudioBuffer& output)
            {
                return HART_LT (abs (maxCrossCorrelation (input, output, 10_ms).get()), 0.8);
            },
            "Weakly correlated"
        )
        .process();

    processAudioWith (HART_DSP_SEQUENCE (Mute() >> AdditiveNoise (0_dB)))
        .withLabel ("Pure noise")
        .withInputSignal (SineSweep())
        .expectTrue (
            [] (const AudioBuffer& input, const AudioBuffer& output)
            {
                return HART_LT (abs (maxCrossCorrelation (input, output, 10_ms).get()), 0.1);
            },
            "Uncorrelated"
        )
        .process();

    processAudioWith (GainDb (-3_dB))
        .withLabel ("Gain doesn't matter")
        .withInputSignal (SineSweep())
        .expectTrue (
            [] (const AudioBuffer& input, const AudioBuffer& output)
            {
                return HART_FLOAT_EQ (maxCrossCorrelation (input, output, 10_ms).get(), 1.0, 1e-3);
            },
            "Perfectly correlated"
        )
        .process();

    processAudioWith (TimeShift (5_ms))
        .withLabel ("Time shift")
        .withInputSignal (SineSweep())
        .expectTrue (
            [] (const AudioBuffer& input, const AudioBuffer& output)
            {
                return HART_GT (maxCrossCorrelation (input, output, 10_ms).get(), 0.999);
            },
            "Lag range larger than time shift"
        )
        .expectTrue (
            [] (const AudioBuffer& input, const AudioBuffer& output)
            {
                return HART_LT (maxCrossCorrelation (input, output, 2_ms).get(), 0.999);
            },
            "Lag range smaller than time shift"
        )
        .process();

    processAudioWith (GainLinear (-1.0))
        .withLabel ("Polarity")
        .withInputSignal (SineSweep())
        .expectTrue (
            [] (const AudioBuffer& input, const AudioBuffer& output)
            {
                return HART_FLOAT_EQ (maxCrossCorrelation (input, output, 10_ms).get(), -1.0, 1e-3);
            },
            "Looking for best abs correlation (default argument)"
        )
        .expectTrue (
            [] (const AudioBuffer& input, const AudioBuffer& output)
            {
                return HART_FLOAT_EQ (maxCrossCorrelation (input, output, 10_ms, hart::bestAbsoluteCorrelation).get(), -1.0, 1e-3);
            },
            "Looking for best abs correlation (explicit argument)"
        )

        .expectTrue (
            [] (const AudioBuffer& input, const AudioBuffer& output)
            {
                return HART_FLOAT_NE (maxCrossCorrelation (input, output, 10_ms, hart::bestSignedCorrelation).get(), -1.0, 1e-3);
            },
            "Looking for best signed correlation (explicit argument)"
        )
        .process();

    processAudioWith (AdditiveNoise (-9_dB))
        .withLabel ("Units")
        .withInputSignal (SineSweep() >> GainDb (-6_dB))
        .expectTrue (
            [] (const AudioBuffer& input, const AudioBuffer& output)
            {
                return HART_FLOAT_EQ (
                    maxCrossCorrelation (input, output, 10_ms).get(),
                    maxCrossCorrelation (input, output, 10_ms).as (none).get(),
                    1e-8
                    );
            },
            "default == unitless"
        )
        .expectTrue (
            [] (const AudioBuffer& input, const AudioBuffer& output)
            {
                return HART_FLOAT_EQ (
                    maxCrossCorrelation (input, output, 10_ms).as (native).get(),
                    maxCrossCorrelation (input, output, 10_ms).as (none).get(),
                    1e-8
                    );
            },
            "native == unitless"
        )
        .process();
}

HART_TEST ("Metrics - Lag At Max Cross Correlation")
{
    using AudioBuffer = hart::AudioBuffer<float>;
    using hart::lagAtMaxCrossCorrelation;
    using std::abs;

    const double defaultSampleRateHz = hart::CLIConfig::getInstance().getDefaultSampleRateHz();
    const double defaultRenderDurationSeconds = hart::CLIConfig::getInstance().getDefaultRenderDurationSeconds();

    processAudioWith (GainDb (0_dB))
        .withLabel ("Identical signal")
        .withInputSignal (SineSweep())
        .expectTrue (
            [] (const AudioBuffer& input, const AudioBuffer& output)
            {
                return HART_FLOAT_EQ (lagAtMaxCrossCorrelation (input, output, 10_ms).get(), 0_s, 1_us);
            },
            "No latency"
        )
        .process();

    for (const double timeShiftSeconds : {2_ms, 5_ms, 15_ms})
    {
        const double renderDurationSeconds = std::max (4 * timeShiftSeconds, defaultRenderDurationSeconds);
        const double expectedLagFrames = timeShiftSeconds * defaultSampleRateHz;

        processAudioWith (TimeShift (timeShiftSeconds))
            .withLabel (HART_STR ("Time shift = " << timeShiftSeconds << " s"))
            .withInputSignal (SineSweep (1_s))
            .withDuration (renderDurationSeconds)
            .expectTrue (
                [timeShiftSeconds] (const AudioBuffer& input, const AudioBuffer& output)
                {
                    return HART_FLOAT_EQ (lagAtMaxCrossCorrelation (input, output, timeShiftSeconds * 2).as (seconds).get(), timeShiftSeconds, 1_ms);
                },
                "Lag range larger than time shift, calculated in seconds"
            )
            .expectTrue (
                [timeShiftSeconds] (const AudioBuffer& input, const AudioBuffer& output)
                {
                    return HART_FLOAT_NE (lagAtMaxCrossCorrelation (input, output, timeShiftSeconds - 1_ms).as (seconds).get(), timeShiftSeconds, 1_ms);
                },
                "Lag range smaller than time shift, calculated in seconds"
            )
            .expectTrue (
                [timeShiftSeconds, expectedLagFrames] (const AudioBuffer& input, const AudioBuffer& output)
                {
                    return HART_FLOAT_EQ (lagAtMaxCrossCorrelation (input, output, timeShiftSeconds * 2).as (frames).get(), expectedLagFrames, 2.0 /* frames */);
                },
                "Lag range larger than time shift, calculated in frames"
            )
            .expectTrue (
                [timeShiftSeconds, expectedLagFrames] (const AudioBuffer& input, const AudioBuffer& output)
                {
                    return HART_FLOAT_NE (lagAtMaxCrossCorrelation (input, output, timeShiftSeconds - 1_ms).as (frames).get(), expectedLagFrames, 2.0 /* frames */);
                },
                "Lag range smaller than time shift, calculated in frames"
            )
            .process();
    }

    processAudioWith (HART_DSP_SEQUENCE (GainLinear (-1.0) >> TimeShift (5_ms)))
        .withLabel ("Flipped signal")
        .withInputSignal (SineSweep())
        .expectTrue (
            [] (const AudioBuffer& input, const AudioBuffer& output)
            {
                return HART_FLOAT_EQ (lagAtMaxCrossCorrelation (input, output, 10_ms, 0.9, hart::bestAbsoluteCorrelation).as (seconds).get(), 5_ms, 500_us);
            },
            "CorrelationSearchMode = bestAbsoluteCorrelation"
        )
        .expectTrue (
            [] (const AudioBuffer& input, const AudioBuffer& output)
            {
                const double lagSeconds = lagAtMaxCrossCorrelation (input, output, 10_ms, 0.9, hart::bestSignedCorrelation).as (seconds).get();
                return HART_TRUE (std::isnan (lagSeconds) || hart::floatsNotEqual (lagSeconds, 5_ms, 500_us));
            },
            "CorrelationSearchMode = bestSignedCorrelation"
        )
        .process();

    processAudioWith (HART_DSP_SEQUENCE (GainDb (-9_dB) >> AdditiveNoise (-3_dB) >> TimeShift (5_ms)))
        .withLabel ("Very noisy signal")
        .withInputSignal (SineSweep())
        .expectTrue (
            [] (const AudioBuffer& input, const AudioBuffer& output)
            {
                return HART_FLOAT_EQ (lagAtMaxCrossCorrelation (input, output, 10_ms, 0.3).as (seconds).get(), 5_ms, 500_us);
            },
            "Correlation threshold = 0.3"
        )
        .expectTrue (
            [] (const AudioBuffer& input, const AudioBuffer& output)
            {
                const double lagSeconds = lagAtMaxCrossCorrelation (input, output, 10_ms, 0.9).as (seconds).get();
                return HART_TRUE (std::isnan (lagSeconds) || hart::floatsNotEqual (lagSeconds, 5_ms, 500_us));
            },
            "Correlation threshold = 0.9"
        )
        .process();

    processAudioWith (TimeShift (1_ms))
        .withLabel ("Units")
        .withInputSignal (SineSweep())
        .expectTrue (
            [] (const AudioBuffer& input, const AudioBuffer& output)
            {
                return HART_FLOAT_EQ (
                    lagAtMaxCrossCorrelation (input, output, 2_ms).get(),
                    lagAtMaxCrossCorrelation (input, output, 2_ms).as (frames).get(),
                    1e-8
                    );
            },
            "default == frames"
        )
        .expectTrue (
            [] (const AudioBuffer& input, const AudioBuffer& output)
            {
                return HART_FLOAT_EQ (
                    lagAtMaxCrossCorrelation (input, output, 2_ms).as (native).get(),
                    lagAtMaxCrossCorrelation (input, output, 2_ms).as (frames).get(),
                    1e-8
                    );
            },
            "native == frames"
        )
        .expectTrue (
            [] (const AudioBuffer& input, const AudioBuffer& output)
            {
                return HART_FLOAT_NE (
                    lagAtMaxCrossCorrelation (input, output, 2_ms).as (frames).get(),
                    lagAtMaxCrossCorrelation (input, output, 2_ms).as (seconds).get(),
                    1e-8
                    );
            },
            "frames != seconds"
        )
        .process();
}

HART_TEST ("Metrics - RMS for AudioBuffer - Matches values for common waveforms")
{
    using AudioBuffer = hart::AudioBuffer<float>;
    using hart::rms;
    using std::sqrt;

    constexpr double invSqrt2 = 0.7071067811865475;
    processAudioWith (GainDb (0_dB))
        .withLabel ("Sine wave")
        .withInputSignal (SineWave ())
        .inMono()
        .expectTrue ([=] (const AudioBuffer& buffer) { return HART_FLOAT_EQ (rms (buffer).get(), invSqrt2, 1e-6); }, "RMS = 1 / sqrt(2)")
        .process();

    processAudioWith (GainDb (0_dB))
        .withLabel ("Sine sweep")
        .withInputSignal (SineSweep ())
        .inMono()
        .expectTrue ([=] (const AudioBuffer& buffer) { return HART_FLOAT_EQ (rms (buffer).get(), invSqrt2, 0.01); }, "RMS ~= 1 / sqrt(2)")
        .process();

    processAudioWith (GainDb (0_dB))
        .withLabel ("Square (ish) wave")
        .withInputSignal (SineWave () >> GainDb (30_dB) >> HardClip())
        .inMono()
        .expectTrue ([] (const AudioBuffer& buffer) { return HART_FLOAT_EQ (rms (buffer).get(), 1.0, 0.01); }, "RMS ~= 1")
        .process();
    
    constexpr double invSqrt3 = 0.5773502691896258;
    processAudioWith (GainDb (0_dB))
        .withLabel ("Band-limited sawtooth")
        .withInputSignal (Sawtooth())
        .inMono()
        .expectTrue ([=] (const AudioBuffer& buffer) { return HART_FLOAT_EQ (rms (buffer).get(), invSqrt3, 0.01); }, "RMS ~= 1 / sqrt(3)")
        .process();
}

HART_TEST ("Metrics - Zero Crossing Rate - Basics")
{
    using AudioBuffer = hart::AudioBuffer<float>;
    using hart::zcr;

    for (const double expectedFrequencyHz : {222_Hz, 440_Hz, 5000_Hz, 15000_Hz})
    {
        processAudioWith (GainDb (0_dB))
            .withLabel ("Sine wave")
            .withInputSignal (SineWave (expectedFrequencyHz))
            .inMono()
            .expectTrue (
                [expectedFrequencyHz]
                    (const AudioBuffer& buffer)
                    {return HART_FREQ_EQ (zcr (buffer).get() * 0.5, expectedFrequencyHz, 25_cents);},
                "1/2 * ZCR = Sine wave frequency, within 2 cents"
                )
            .process();

        processAudioWith (GainDb (0_dB))
            .withLabel ("Sawtooth")
            .withInputSignal (Sawtooth (expectedFrequencyHz))
            .inMono()
            .expectTrue (
                [expectedFrequencyHz]
                    (const AudioBuffer& buffer)
                    {return HART_FREQ_EQ (zcr (buffer).get() * 0.5, expectedFrequencyHz, 25_cents);},
                "1/2 * ZCR = Sawtooth frequency, within 2 cents"
                )
            .process();
    }

    const double sampleRateHz = hart::CLIConfig::getInstance().getDefaultSampleRateHz();
    processAudioWith (GainDb (0_dB))
        .withLabel ("Nyquist signal")
        .withInputSignal (NyquistSignal())
        .inMono()
        .expectTrue (
            [sampleRateHz]
                (const AudioBuffer& buffer)
                {return HART_FREQ_EQ (zcr (buffer).get(), sampleRateHz, 2_cents);},
            "ZCR = Sample Rate, within 2 cents"
            )
        .process();

    processAudioWith (GainDb (0_dB))
        .withLabel ("Silence")
        .withInputSignal (Silence())
        .inMono()
        .expectTrue (
            [] (const AudioBuffer& buffer)
                { return HART_FLOAT_EQ (zcr (buffer).get(), 0.0, 1e-8); },
            "ZCR = 0"
            )
        .process();

    processAudioWith (GainDb (0_dB))
        .withLabel ("DC signal")
        .withInputSignal (DC (1.0f))
        .inMono()
        .expectTrue (
            [] (const AudioBuffer& buffer)
                { return HART_FLOAT_EQ (zcr (buffer).get(), 0.0, 1e-8); },
            "ZCR = 0"
            )
        .process();

    processAudioWith (GainDb (0_dB))
        .withLabel ("Unit")
        .withInputSignal (SineWave (1234_Hz))
        .inMono()
        .expectTrue (
            [] (const AudioBuffer& buffer)
            {
                return HART_FLOAT_EQ (
                    zcr (buffer).as (native).get(),
                    zcr (buffer).as (Hz).get(),
                    1e-8
                    );
            },
            "Native unit is Hz"
            )
        .process();
}

HART_TEST ("Metrics - Zero Crossing Rate - Very quiet noise detection")
{
    using AudioBuffer = hart::AudioBuffer<float>;
    using hart::zcr;

    const double sampleRateHz = hart::CLIConfig::getInstance().getDefaultSampleRateHz();
    processAudioWith (GainDb (0_dB))
        .withLabel ("Unit")
        .withInputSignal (Silence() >> AdditiveNoise (0_dB) >> GainLinear (1e-10))  // -200 dB sample peak
        .inMono()
        .expectTrue (EqualsTo (Silence(), 1e-6))  // -120 dB threshold - goes under the radar
        .expectTrue (
            [] (const AudioBuffer& buffer)
                { return HART_FLOAT_NE (zcr (buffer).get(), 0.0, 1e-6); },
            "ZCR is not zero..."
            )
        .expectTrue (
            [sampleRateHz] (const AudioBuffer& buffer)
                { return HART_GT (zcr (buffer).get(), 0.1 * sampleRateHz); },
            "...in fact, ZCR is pretty high!"
            )
        .process();
}

HART_TEST ("Metrics - True Peak")
{
    using hart::truePeak;
    using hart::samplePeak;
    using hart::min;
    using hart::max;
    using hart::range;
    using AudioBuffer = hart::AudioBuffer<float>;
    using FilterQuality = hart::TruePeak<float>::FilterQuality;
    using Oversampling = hart::Oversampling;

    for (const double levelDb : {-12_dB, -3_dB, -0.5_dB, 0_dB, 3_dB})
    {
        processAudioWith (GainDb (levelDb))
            .withLabel ("Sine wave's sample peaks ~= true peaks")
            .withInputSignal (SineWave())
            .inMono()
            .assertTrue (PeaksAt (levelDb))
            .expectTrue ([] (const AudioBuffer& output) { return HART_FLOAT_EQ (truePeak (output).as (dB).get(), samplePeak (output).as (dB).get(), 0.01); }, "True peak ~= sample peak")
            .expectTrue ([] (const AudioBuffer& output) { return HART_FLOAT_EQ (truePeak (output, Oversampling::x8, FilterQuality::medium).as (dB).get(), samplePeak (output).as (dB).get(), 0.01); }, "True peak ~= sample peak")
            .expectTrue ([] (const AudioBuffer& output) { return HART_FLOAT_EQ (truePeak (output, Oversampling::x16, FilterQuality::high).as (dB).get(), samplePeak (output).as (dB).get(), 0.01); }, "True peak ~= sample peak")
            .process();

        processAudioWith (GainDb (levelDb))
            .withLabel ("White Noise true peaks way above sample peaks")
            .withInputSignal (WhiteNoise())
            .expectTrue (PeaksAt (levelDb))
            .expectTrue ([] (const AudioBuffer& output) { return HART_GT (truePeak (output).as (dB).get(), 0.1_dB + samplePeak (output).as (dB).get()); }, "True peak > sample peak + 0.1dB")
            .expectTrue ([] (const AudioBuffer& output) { return HART_GT (truePeak (output, Oversampling::x8, FilterQuality::medium).as (dB).get(), 0.1_dB + samplePeak (output).as (dB).get()); }, "True peak > sample peak + 0.1dB")
            .expectTrue ([] (const AudioBuffer& output) { return HART_GT (truePeak (output, Oversampling::x16, FilterQuality::high).as (dB).get(), 0.1_dB + samplePeak (output).as (dB).get()); }, "True peak > sample peak + 0.1dB")
            .process();
    }

    auto multiChannelDSP = HART_DSP_SEQUENCE (
        GainDb (-3_dB).atChannels ({0, 1})
        >> GainDb (-6_dB).atChannel (2)
        >> GainDb (-12_dB).atChannel (3)
        >> GainDb (+3_dB).atChannel (4)
        );

    processAudioWith (std::move (multiChannelDSP))
        .withLabel ("Multi-channel")
        .withInputSignal (SineWave())
        .withInputChannels (5)
        .withOutputChannels (5)
        .expectTrue ([] (const AudioBuffer& output) { return HART_FLOAT_EQ (truePeak (output).as (dB).get (range()), +15_dB, 0.01); }, "All channels - Range of TP values")
        .expectTrue ([] (const AudioBuffer& output) { return HART_FLOAT_EQ (truePeak (output).ch (2).as (dB).get(), -6_dB, 0.01); }, "Channel 2")
        .expectTrue ([] (const AudioBuffer& output) { return HART_FLOAT_EQ (truePeak (output).ch (3).as (dB).get(), -12_dB, 0.01); }, "Channel 3")
        .expectTrue ([] (const AudioBuffer& output) { return HART_FLOAT_EQ (truePeak (output).as (dB).get (min()), -12_dB, 0.01); }, "Lowest TP of all channels")
        .expectTrue ([] (const AudioBuffer& output) { return HART_FLOAT_EQ (truePeak (output).as (dB).get (max()), +3_dB, 0.01); }, "Highest TP of all channels")
        .expectTrue ([] (const AudioBuffer& output) { return HART_FLOAT_EQ (truePeak (output).ch ({2, 3}).as (dB).get (max()), -6_dB, 0.01); }, "Lowest TP of channels 2 and 3")
        .process();

    // TODO: Units
    // TODO: Slices
}

HART_TEST ("Metrics - SNR")
{
    using std::isinf;
    using hart::snr;
    using hart::min;
    using AnalysisContext = hart::AnalysisContext<float>;

    // Sine wave an unity gain has RMS of 1 / sqrt (2)
    // White noise (uniform distribution) at about -18.2dB sample peak has RMS of 1 / (10 * sqrt (2))
    // Mixing those resulta in 20 dB SNR
    processAudioWith (AdditiveNoise (-18.2_dB))
        .withLabel ("Additive noise")
        .withInputSignal (SineWave())
        .expectTrue ([] (AnalysisContext ac) { return HART_FLOAT_EQ (snr (ac.inputAudio(), ac.outputAudio()).as (dB).get (min()), 20_dB, 0.1_dB); }, "SNR is ~20 dB" )
        .process();

    processAudioWith (GainDb (0_dB))
        .withLabel ("No noise at all")
        .withInputSignal (SineWave())
        .expectTrue ([] (AnalysisContext ac) { return HART_GT (snr (ac.inputAudio(), ac.outputAudio()).as (dB).get (min()), 100_dB); }, "SNR > 100 dB" )
        .expectTrue ([] (AnalysisContext ac) { return HART_TRUE (isinf (snr (ac.inputAudio(), ac.outputAudio()).as (dB).get (min()))); }, "SNR -> +oo" )
        .process();

    processAudioWith (GainDb (-10_dB))
        .withLabel ("Mismatched gain throws off SNR reading")
        .withInputSignal (SineWave())
        .expectTrue ([] (AnalysisContext ac) { return HART_LT (snr (ac.inputAudio(), ac.outputAudio()).as (dB).get (min()), 100_dB); }, "SNR < 100 dB" )
        .process();

    processAudioWith (TimeShift (1_ms))
        .withLabel ("Latency throws off SNR reading")
        .withInputSignal (SineWave())
        .expectTrue ([] (AnalysisContext ac) { return HART_LT (snr (ac.inputAudio(), ac.outputAudio()).as (dB).get (min()), 100_dB); }, "SNR < 100 dB" )
        .process();
}

HART_TEST ("Metrics - SNR - Units")
{
    using hart::snr;
    using hart::powerToDecibels;
    using AudioBuffer = hart::AudioBuffer<float>;

    AudioBuffer signal;
    AudioBuffer signalPlusNoise;
    processAudioWith (AdditiveNoise (-10_dB))
        .withInputSignal (SineWave())
        .saveInputTo (signal)
        .saveOutputTo (signalPlusNoise)
        .inMono()
        .process();

    const double snrDefault = snr (signal, signalPlusNoise);
    const double snrNative = snr (signal, signalPlusNoise).as (native);
    const double snrRatio = snr (signal, signalPlusNoise).as (ratio);
    const double snrDb = snr (signal, signalPlusNoise).as (dB);

    HART_EXPECT_FLOAT_EQ (snrNative, snrDefault, 1e-16) << "Native is the default";
    HART_EXPECT_FLOAT_EQ (snrRatio, snrNative, 1e-16) << "Ratio is the same as Native";
    HART_EXPECT_FLOAT_EQ (snrDb, powerToDecibels (snrRatio), 1e-16) << "Decibels are calculated as power, and not voltage";
}

HART_TEST ("Metrics - SNRa - Distortion effect with aliasing")
{
    using hart::snra;
    using hart::max;
    using hart::min;
    using hart::Slice;
    using AudioBuffer = hart::AudioBuffer<float>;

    // Resampling the reference buffer doesn't guarantee preserving the transients at boundaries
    // (i.e. the very beginning and very and of the buffer), so it's a good idea to only compare the
    // slice of audio in the middle.
    const Slice steadySlice = Slice::time (50_ms, 150_ms);
    constexpr double renderDurationSeconds = 200_ms;

    const double nativeSampleRateHz = hart::CLIConfig::getInstance().getDefaultSampleRateHz();
    const double highSampleRateHz = 4 * nativeSampleRateHz;
    std::vector<double> snras;

    for (double sineFrequencyHz = 20_Hz; sineFrequencyHz <= 20_kHz; sineFrequencyHz *= 2)
    {
        AudioBuffer estimatedBuffer;
        processAudioWith (HardClip (-10_dB))
            .withInputSignal (SineWave (sineFrequencyHz))
            .withDuration (renderDurationSeconds)
            .saveOutputTo (estimatedBuffer)
            .process();

        AudioBuffer referenceBuffer;
        processAudioWith (HardClip (-10_dB))
            .withInputSignal (SineWave (sineFrequencyHz))
            .withSampleRate (highSampleRateHz)
            .withDuration (renderDurationSeconds)
            .saveOutputTo (referenceBuffer)
            .process();
        
        snras.push_back (snra (estimatedBuffer, referenceBuffer).at (steadySlice).as (dB). get (min()));
    }

    HART_EXPECT_LT (max() (snras.begin(), snras.end()), 100_dB) << "Best SNRa < 100 dB";
    HART_EXPECT_LT (min() (snras.begin(), snras.end()), 30_dB) << "Worst SNRa < 30 dB";
}

HART_TEST ("Metrics - SNRa - Clean effect with no aliasing")
{
    using hart::snra;
    using hart::min;
    using hart::Slice;
    using AudioBuffer = hart::AudioBuffer<float>;

    // Resampling the reference buffer doesn't guarantee preserving the transients at boundaries
    // (i.e. the very beginning and very and of the buffer), so it's a good idea to only compare the
    // slice of audio in the middle.
    const Slice steadySlice = Slice::time (50_ms, 150_ms);
    constexpr double renderDurationSeconds = 200_ms;

    const double nativeSampleRateHz = hart::CLIConfig::getInstance().getDefaultSampleRateHz();
    const double highSampleRateHz = 4 * nativeSampleRateHz;
    std::vector<double> snras;

    for (double sineFrequencyHz = 20_Hz; sineFrequencyHz <= 20_kHz; sineFrequencyHz *= 2)
    {
        AudioBuffer estimatedBuffer;
        processAudioWith (GainDb (-10_dB))
            .withInputSignal (SineWave (sineFrequencyHz))
            .withDuration (renderDurationSeconds)
            .saveOutputTo (estimatedBuffer)
            .process();

        AudioBuffer referenceBuffer;
        processAudioWith (GainDb (-10_dB))
            .withInputSignal (SineWave (sineFrequencyHz))
            .withSampleRate (highSampleRateHz)
            .withDuration (renderDurationSeconds)
            .saveOutputTo (referenceBuffer)
            .process();

        snras.push_back (snra (estimatedBuffer, referenceBuffer).at (steadySlice).as (dB).get (min()));
    }
    
    HART_EXPECT_GT (min() (snras.begin(), snras.end()), 100_dB) << "Worst SNRa > 100 dB";
}

HART_TEST ("Metrics - SNRa - Units")
{
    using hart::snra;
    using hart::powerToDecibels;
    using AudioBuffer = hart::AudioBuffer<float>;

    const double nativeSampleRateHz = hart::CLIConfig::getInstance().getDefaultSampleRateHz();
    const double highSampleRateHz = 4 * nativeSampleRateHz;

    AudioBuffer estimatedBuffer;
    processAudioWith (HardClip (-10_dB))
        .withInputSignal (SineWave())
        .saveOutputTo (estimatedBuffer)
        .inMono()
        .process();

    AudioBuffer referenceBuffer;
    processAudioWith (HardClip (-10_dB))
        .withInputSignal (SineWave())
        .withSampleRate (highSampleRateHz)
        .saveOutputTo (referenceBuffer)
        .inMono()
        .process();
        
    const double snraDefault = snra (estimatedBuffer, referenceBuffer);
    const double snraNative = snra (estimatedBuffer, referenceBuffer).as (native);
    const double snraRatio = snra (estimatedBuffer, referenceBuffer).as (ratio);
    const double snraDb = snra (estimatedBuffer, referenceBuffer).as (dB);

    HART_EXPECT_FLOAT_EQ (snraNative, snraDefault, 1e-16) << "Native is the default";
    HART_EXPECT_FLOAT_EQ (snraRatio, snraNative, 1e-16) << "Ratio is the same as Native";
    HART_EXPECT_FLOAT_EQ (snraDb, powerToDecibels (snraRatio), 1e-16) << "Decibels are calculated as power, and not voltage";
}

HART_TEST_WITH_TAGS ("Metrics - LUFS - Silence reads at under -100 LUFS", "[Metrics][LUFS]")
{
    using hart::lufs;
    using hart::LUFS::integrated;
    using hart::LUFS::shortTerm;
    using hart::LUFS::momentary;

    const AudioBuffer audio = processAudioWith (Bypass())
        .withInputSignal (Silence())
        .withDuration (3_s)
        .processAndGetOutputAudio();

    HART_EXPECT_LT (lufs (audio).ch (hart::left).get (integrated()), -100_LUFS);
    HART_EXPECT_LT (lufs (audio).ch (hart::left).get (shortTerm()), -100_LUFS);
    HART_EXPECT_LT (lufs (audio).ch (hart::left).get (momentary()), -100_LUFS);
}

// Reference: EBU Tech 3341, section 2.9, Table 1.
// https://tech.ebu.ch/docs/tech/tech3341.pdf

HART_TEST_WITH_TAGS ("Metrics - LUFS - Satisfies requirements for EBU Tech 3341 test case #1", "[Metrics][LUFS]")
{
    using hart::lufs;
    using hart::LUFS::integrated;
    using hart::LUFS::shortTerm;
    using hart::LUFS::momentary;

    // Approximates seq-3341-1-16bit.wav from EBU Tech 3341 test case #1.
    // Uses the specified pure tone without the original recording's residual.
    const AudioBuffer audio = processAudioWith (Bypass())
        .withInputSignal (SineWave (1_kHz) >> GainDb (-23_dB))
        .withDuration (20_s)
        .withSampleRate (48_kHz)
        .inStereo()
        .processAndGetOutputAudio();

    HART_EXPECT_FLOAT_EQ (lufs (audio).get (integrated()), -23_LUFS, 0.1_LU);
    HART_EXPECT_FLOAT_EQ (lufs (audio).get (shortTerm()), -23_LUFS, 0.1_LU);
    HART_EXPECT_FLOAT_EQ (lufs (audio).get (momentary()), -23_LUFS, 0.1_LU);

    // Table 1 also gives the readings relative to the EBU -23 LUFS target.
    HART_EXPECT_FLOAT_EQ (lufs (audio).as (LU).get (integrated()), 0_LU, 0.1_LU);
    HART_EXPECT_FLOAT_EQ (lufs (audio).as (LU).get (shortTerm()), 0_LU, 0.1_LU);
    HART_EXPECT_FLOAT_EQ (lufs (audio).as (LU).get (momentary()), 0_LU, 0.1_LU);
}

HART_TEST_WITH_TAGS ("Metrics - LUFS - Satisfies requirements for EBU Tech 3341 test case #2", "[Metrics][LUFS]")
{
    using hart::lufs;
    using hart::LUFS::integrated;
    using hart::LUFS::shortTerm;
    using hart::LUFS::momentary;

    // Approximates seq-3341-2-16bit.wav from EBU Tech 3341 test case #2.
    // Uses the specified pure tone without the original recording's residual.
    const AudioBuffer audio = processAudioWith (Bypass())
        .withInputSignal (SineWave (1_kHz) >> GainDb (-33_dB))
        .withDuration (20_s)
        .withSampleRate (48_kHz)
        .inStereo()
        .processAndGetOutputAudio();

    HART_EXPECT_FLOAT_EQ (lufs (audio).get (integrated()), -33_LUFS, 0.1_LU);
    HART_EXPECT_FLOAT_EQ (lufs (audio).get (shortTerm()), -33_LUFS, 0.1_LU);
    HART_EXPECT_FLOAT_EQ (lufs (audio).get (momentary()), -33_LUFS, 0.1_LU);

    // Table 1 also gives the readings relative to the EBU -23 LUFS target.
    HART_EXPECT_FLOAT_EQ (lufs (audio).as (LU).get (integrated()), -10_LU, 0.1_LU);
    HART_EXPECT_FLOAT_EQ (lufs (audio).as (LU).get (shortTerm()), -10_LU, 0.1_LU);
    HART_EXPECT_FLOAT_EQ (lufs (audio).as (LU).get (momentary()), -10_LU, 0.1_LU);
}

HART_TEST_WITH_TAGS ("Metrics - LUFS - Satisfies requirements for EBU Tech 3341 test case #3", "[Metrics][LUFS]")
{
    using hart::lufs;
    using hart::LUFS::integrated;

    // Approximates seq-3341-3-16bit-v02.wav from EBU Tech 3341 test case #3.
    // SegmentedEnvelope currently applies level changes one sample early.
    const auto volumeEnvelope = SegmentedEnvelope (-36_dB)
        .hold (10_s)
        .rampTo (-23_dB, 0_s)
        .hold (60_s)
        .rampTo (-36_dB, 0_s);

    auto synthesisedSignal = SineWave (1_kHz)
        >> GainDb().withEnvelope (GainDb::gainDb, volumeEnvelope);

    const AudioBuffer audio = processAudioWith (Bypass())
        .withInputSignal (std::move (synthesisedSignal))
        .withDuration (80_s)
        .withSampleRate (48_kHz)
        .inStereo()
        .processAndGetOutputAudio();

    HART_EXPECT_FLOAT_EQ (lufs (audio).get (integrated()), -23_LUFS, 0.1_LU);
}

HART_TEST_WITH_TAGS ("Metrics - LUFS - Satisfies requirements for EBU Tech 3341 test case #4", "[Metrics][LUFS]")
{
    using hart::lufs;
    using hart::LUFS::integrated;

    // Approximates seq-3341-4-16bit-v02.wav from EBU Tech 3341 test case #4.
    // SegmentedEnvelope currently applies level changes one sample early.
    const auto volumeEnvelope = SegmentedEnvelope (-72_dB)
        .hold (10_s)
        .rampTo (-36_dB, 0_s)
        .hold (10_s)
        .rampTo (-23_dB, 0_s)
        .hold (60_s)
        .rampTo (-36_dB, 0_s)
        .hold (10_s)
        .rampTo (-72_dB, 0_s);

    auto synthesisedSignal = SineWave (1_kHz)
        >> GainDb().withEnvelope (GainDb::gainDb, volumeEnvelope);

    const AudioBuffer audio = processAudioWith (Bypass())
        .withInputSignal (std::move (synthesisedSignal))
        .withDuration (100_s)
        .withSampleRate (48_kHz)
        .inStereo()
        .processAndGetOutputAudio();

    HART_EXPECT_FLOAT_EQ (lufs (audio).get (integrated()), -23_LUFS, 0.1_LU);
}

HART_TEST_WITH_TAGS ("Metrics - LUFS - Satisfies requirements for EBU Tech 3341 test case #5", "[Metrics][LUFS]")
{
    using hart::lufs;
    using hart::LUFS::integrated;

    // Approximates seq-3341-5-16bit-v02.wav from EBU Tech 3341 test case #5.
    // SegmentedEnvelope currently applies level changes one sample early.
    const auto volumeEnvelope = SegmentedEnvelope (-26_dB)
        .hold (20_s)
        .rampTo (-20_dB, 0_s)
        .hold (20.1_s)
        .rampTo (-26_dB, 0_s);

    auto synthesisedSignal = SineWave (1_kHz)
        >> GainDb().withEnvelope (GainDb::gainDb, volumeEnvelope);

    const AudioBuffer audio = processAudioWith (Bypass())
        .withInputSignal (std::move (synthesisedSignal))
        .withDuration (60.1_s)
        .withSampleRate (48_kHz)
        .inStereo()
        .processAndGetOutputAudio();

    HART_EXPECT_FLOAT_EQ (lufs (audio).get (integrated()), -23_LUFS, 0.1_LU);
}

HART_TEST_WITH_TAGS ("Metrics - LUFS - Satisfies requirements for EBU Tech 3341 test case #6 (5.0)", "[Metrics][LUFS]")
{
    using hart::lufs;
    using hart::LUFS::integrated;

    // Replicates the signal specification of seq-3341-6-5channels-16bit.wav from EBU Tech 3341 test case #6 (5.0).
    // Generated directly, without PCM16 quantization.
    // Channel order: L/R/C/Ls/Rs, with respective peak levels -28/-28/-24/-30/-30 dBFS.
    auto synthesisedSignal = SineWave (1_kHz)
        >> GainDb (-28_dB).atChannels ({ 0, 1 })
        >> GainDb (-24_dB).atChannel (2)
        >> GainDb (-30_dB).atChannels ({ 3, 4 });

    const AudioBuffer audio = processAudioWith (Bypass())
        .withInputSignal (std::move (synthesisedSignal))
        .withDuration (20_s)
        .withSampleRate (48_kHz)
        .withInputChannels (5)
        .withOutputChannels (5)
        .processAndGetOutputAudio();

    // Channel order: L/R/C/Ls/Rs.
    HART_EXPECT_FLOAT_EQ (lufs (audio).get (integrated()), -23_LUFS, 0.1_LU);
}

HART_TEST_WITH_TAGS ("Metrics - LUFS - Satisfies requirements for EBU Tech 3341 test case #6 (5.1)", "[Metrics][LUFS]")
{
    using hart::lufs;
    using hart::LUFS::integrated;

    // Replicates the signal specification of seq-3341-6-6channels-WAVEEX-16bit.wav from EBU Tech 3341 test case #6 (5.1).
    // Generated directly, without PCM16 quantization.
    // Channel order: L/R/C/LFE/Ls/Rs. LFE is silent; the other levels match the 5.0 file.
    auto synthesisedSignal = SineWave (1_kHz)
        >> GainDb (-28_dB).atChannels ({ 0, 1 })
        >> GainDb (-24_dB).atChannel (2)
        >> Mute().atChannel (3)
        >> GainDb (-30_dB).atChannels ({ 4, 5 });

    const AudioBuffer audio = processAudioWith (Bypass())
        .withInputSignal (std::move (synthesisedSignal))
        .withDuration (20_s)
        .withSampleRate (48_kHz)
        .withInputChannels (6)
        .withOutputChannels (6)
        .processAndGetOutputAudio();

    // Channel order: L/R/C/LFE/Ls/Rs.
    HART_EXPECT_FLOAT_EQ (lufs (audio).get (integrated()), -23_LUFS, 0.1_LU);
}

HART_TEST_WITH_TAGS ("Metrics - LUFS - Satisfies requirements for EBU Tech 3341 test case #9", "[Metrics][LUFS]")
{
    using hart::lufs;
    using hart::LUFS::shortTerm;

    // Approximates seq-3341-9-24bit.wav from EBU Tech 3341 test case #9.
    // Repeat a 3 s cycle five times: 1.34 s at -20 dBFS, then 1.66 s at -30 dBFS.
    const auto volumeEnvelope = SegmentedEnvelope (-20_dB)
        .hold (1.34_s)
        .rampTo (-30_dB, 0_s);

    AudioBuffer cycle = processAudioWith (Bypass())
        .withInputSignal (SineWave (1_kHz) >> GainDb().withEnvelope (GainDb::gainDb, volumeEnvelope))
        .withDuration (3_s)
        .withSampleRate (48_kHz)
        .inStereo()
        .processAndGetOutputAudio();

    const AudioBuffer audio = processAudioWith (Bypass())
        .withInputSignal (std::move (cycle).toSignal (hart::Loop::yes))
        .withDuration (15_s)
        .withSampleRate (48_kHz)
        .inStereo()
        .processAndGetOutputAudio();

    // Both extrema must match: S stays constant once the first 3 s window is complete.
    const auto query = lufs (audio);
    HART_EXPECT_FLOAT_EQ (query.get (shortTerm (hart::min())), -23_LUFS, 0.1_LU);
    HART_EXPECT_FLOAT_EQ (query.get (shortTerm()), -23_LUFS, 0.1_LU);
}

HART_PARAMETRISED_TEST_WITH_TAGS ("Metrics - LUFS - Satisfies requirements for EBU Tech 3341 test case #10", "[Metrics][LUFS]")
{
    using hart::lufs;
    using hart::LUFS::shortTerm;

    // Approximates seq-3341-10-{i + 1}-24bit.wav from EBU Tech 3341 test case #10.
    // Each generated i corresponds to a separate reference file, numbered 1 through 20.
    const double i = HART_GENERATE_VALUE (
        0.0, 1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0,
        10.0, 11.0, 12.0, 13.0, 14.0, 15.0, 16.0, 17.0, 18.0, 19.0
        );
    const double silenceDurationSeconds = 150_ms * i;
    const double renderDurationSeconds = silenceDurationSeconds + 3_s + 1_s;

    const auto volumeEnvelope = SegmentedEnvelope (-oo_dB)
        .hold (silenceDurationSeconds)
        .rampTo (-23_dB, 0_s)
        .hold (3_s)
        .rampTo (-oo_dB, 0_s);

    const AudioBuffer audio = processAudioWith (Bypass())
        .withInputSignal (SineWave (1_kHz) >> GainDb().withEnvelope (GainDb::gainDb, volumeEnvelope))
        .withDuration (renderDurationSeconds)
        .withSampleRate (48_kHz)
        .inStereo()
        .processAndGetOutputAudio();

    HART_EXPECT_FLOAT_EQ (lufs (audio).get (shortTerm()), -23_LUFS, 0.1_LU);
}

HART_TEST_WITH_TAGS ("Metrics - LUFS - Satisfies short-term maximum requirements for EBU Tech 3341 test case #11", "[Metrics][LUFS]")
{
    using hart::lufs;
    using hart::LUFS::shortTerm;

    // Approximates seq-3341-11-24bit.wav from EBU Tech 3341 test case #11.
    // Twenty 6 s segments: i * 150 ms silence, 3 s at (-38 + i) dBFS, then remaining silence.
    const auto volumeEnvelope = SegmentedEnvelope (-oo_dB)
        // Segment 1.
        .hold (0_ms)
        .rampTo (-38_dB, 0_s)
        .hold (3_s)
        .rampTo (-oo_dB, 0_s)
        .hold (3000_ms)
        // Segment 2.
        .hold (150_ms)
        .rampTo (-37_dB, 0_s)
        .hold (3_s)
        .rampTo (-oo_dB, 0_s)
        .hold (2850_ms)
        // Segment 3.
        .hold (300_ms)
        .rampTo (-36_dB, 0_s)
        .hold (3_s)
        .rampTo (-oo_dB, 0_s)
        .hold (2700_ms)
        // Segment 4.
        .hold (450_ms)
        .rampTo (-35_dB, 0_s)
        .hold (3_s)
        .rampTo (-oo_dB, 0_s)
        .hold (2550_ms)
        // Segment 5.
        .hold (600_ms)
        .rampTo (-34_dB, 0_s)
        .hold (3_s)
        .rampTo (-oo_dB, 0_s)
        .hold (2400_ms)
        // Segment 6.
        .hold (750_ms)
        .rampTo (-33_dB, 0_s)
        .hold (3_s)
        .rampTo (-oo_dB, 0_s)
        .hold (2250_ms)
        // Segment 7.
        .hold (900_ms)
        .rampTo (-32_dB, 0_s)
        .hold (3_s)
        .rampTo (-oo_dB, 0_s)
        .hold (2100_ms)
        // Segment 8.
        .hold (1050_ms)
        .rampTo (-31_dB, 0_s)
        .hold (3_s)
        .rampTo (-oo_dB, 0_s)
        .hold (1950_ms)
        // Segment 9.
        .hold (1200_ms)
        .rampTo (-30_dB, 0_s)
        .hold (3_s)
        .rampTo (-oo_dB, 0_s)
        .hold (1800_ms)
        // Segment 10.
        .hold (1350_ms)
        .rampTo (-29_dB, 0_s)
        .hold (3_s)
        .rampTo (-oo_dB, 0_s)
        .hold (1650_ms)
        // Segment 11.
        .hold (1500_ms)
        .rampTo (-28_dB, 0_s)
        .hold (3_s)
        .rampTo (-oo_dB, 0_s)
        .hold (1500_ms)
        // Segment 12.
        .hold (1650_ms)
        .rampTo (-27_dB, 0_s)
        .hold (3_s)
        .rampTo (-oo_dB, 0_s)
        .hold (1350_ms)
        // Segment 13.
        .hold (1800_ms)
        .rampTo (-26_dB, 0_s)
        .hold (3_s)
        .rampTo (-oo_dB, 0_s)
        .hold (1200_ms)
        // Segment 14.
        .hold (1950_ms)
        .rampTo (-25_dB, 0_s)
        .hold (3_s)
        .rampTo (-oo_dB, 0_s)
        .hold (1050_ms)
        // Segment 15.
        .hold (2100_ms)
        .rampTo (-24_dB, 0_s)
        .hold (3_s)
        .rampTo (-oo_dB, 0_s)
        .hold (900_ms)
        // Segment 16.
        .hold (2250_ms)
        .rampTo (-23_dB, 0_s)
        .hold (3_s)
        .rampTo (-oo_dB, 0_s)
        .hold (750_ms)
        // Segment 17.
        .hold (2400_ms)
        .rampTo (-22_dB, 0_s)
        .hold (3_s)
        .rampTo (-oo_dB, 0_s)
        .hold (600_ms)
        // Segment 18.
        .hold (2550_ms)
        .rampTo (-21_dB, 0_s)
        .hold (3_s)
        .rampTo (-oo_dB, 0_s)
        .hold (450_ms)
        // Segment 19.
        .hold (2700_ms)
        .rampTo (-20_dB, 0_s)
        .hold (3_s)
        .rampTo (-oo_dB, 0_s)
        .hold (300_ms)
        // Segment 20.
        .hold (2850_ms)
        .rampTo (-19_dB, 0_s)
        .hold (3_s)
        .rampTo (-oo_dB, 0_s)
        .hold (150_ms);

    const AudioBuffer audio = processAudioWith (Bypass())
        .withInputSignal (SineWave (1_kHz) >> GainDb().withEnvelope (GainDb::gainDb, volumeEnvelope))
        .withDuration (120_s)
        .withSampleRate (48_kHz)
        .inStereo()
        .processAndGetOutputAudio();

    // Offline analogue of the successive live readings; this does not test live meter controls.
    HART_EXPECT_FLOAT_EQ (
        lufs (audio).at (hart::Slice::time (0_s, 6_s)).get (shortTerm()),
        -38_LUFS,
        0.1_LU
        )
        << "LUFS-S at segment 1 == -38 LUFS";

    HART_EXPECT_FLOAT_EQ (
        lufs (audio).at (hart::Slice::time (6_s, 12_s)).get (shortTerm()),
        -37_LUFS,
        0.1_LU
        )
        << "LUFS-S at segment 2 == -37 LUFS";

    HART_EXPECT_FLOAT_EQ (
        lufs (audio).at (hart::Slice::time (12_s, 18_s)).get (shortTerm()),
        -36_LUFS,
        0.1_LU
        )
        << "LUFS-S at segment 3 == -36 LUFS";

    HART_EXPECT_FLOAT_EQ (
        lufs (audio).at (hart::Slice::time (18_s, 24_s)).get (shortTerm()),
        -35_LUFS,
        0.1_LU
        )
        << "LUFS-S at segment 4 == -35 LUFS";

    HART_EXPECT_FLOAT_EQ (
        lufs (audio).at (hart::Slice::time (24_s, 30_s)).get (shortTerm()),
        -34_LUFS,
        0.1_LU
        )
        << "LUFS-S at segment 5 == -34 LUFS";

    HART_EXPECT_FLOAT_EQ (
        lufs (audio).at (hart::Slice::time (30_s, 36_s)).get (shortTerm()),
        -33_LUFS,
        0.1_LU
        )
        << "LUFS-S at segment 6 == -33 LUFS";

    HART_EXPECT_FLOAT_EQ (
        lufs (audio).at (hart::Slice::time (36_s, 42_s)).get (shortTerm()),
        -32_LUFS,
        0.1_LU
        )
        << "LUFS-S at segment 7 == -32 LUFS";

    HART_EXPECT_FLOAT_EQ (
        lufs (audio).at (hart::Slice::time (42_s, 48_s)).get (shortTerm()),
        -31_LUFS,
        0.1_LU
        )
        << "LUFS-S at segment 8 == -31 LUFS";

    HART_EXPECT_FLOAT_EQ (
        lufs (audio).at (hart::Slice::time (48_s, 54_s)).get (shortTerm()),
        -30_LUFS,
        0.1_LU
        )
        << "LUFS-S at segment 9 == -30 LUFS";

    HART_EXPECT_FLOAT_EQ (
        lufs (audio).at (hart::Slice::time (54_s, 60_s)).get (shortTerm()),
        -29_LUFS,
        0.1_LU
        )
        << "LUFS-S at segment 10 == -29 LUFS";

    HART_EXPECT_FLOAT_EQ (
        lufs (audio).at (hart::Slice::time (60_s, 66_s)).get (shortTerm()),
        -28_LUFS,
        0.1_LU
        )
        << "LUFS-S at segment 11 == -28 LUFS";

    HART_EXPECT_FLOAT_EQ (
        lufs (audio).at (hart::Slice::time (66_s, 72_s)).get (shortTerm()),
        -27_LUFS,
        0.1_LU
        )
        << "LUFS-S at segment 12 == -27 LUFS";

    HART_EXPECT_FLOAT_EQ (
        lufs (audio).at (hart::Slice::time (72_s, 78_s)).get (shortTerm()),
        -26_LUFS,
        0.1_LU
        )
        << "LUFS-S at segment 13 == -26 LUFS";

    HART_EXPECT_FLOAT_EQ (
        lufs (audio).at (hart::Slice::time (78_s, 84_s)).get (shortTerm()),
        -25_LUFS,
        0.1_LU
        )
        << "LUFS-S at segment 14 == -25 LUFS";

    HART_EXPECT_FLOAT_EQ (
        lufs (audio).at (hart::Slice::time (84_s, 90_s)).get (shortTerm()),
        -24_LUFS,
        0.1_LU
        )
        << "LUFS-S at segment 15 == -24 LUFS";

    HART_EXPECT_FLOAT_EQ (
        lufs (audio).at (hart::Slice::time (90_s, 96_s)).get (shortTerm()),
        -23_LUFS,
        0.1_LU
        )
        << "LUFS-S at segment 16 == -23 LUFS";

    HART_EXPECT_FLOAT_EQ (
        lufs (audio).at (hart::Slice::time (96_s, 102_s)).get (shortTerm()),
        -22_LUFS,
        0.1_LU
        )
        << "LUFS-S at segment 17 == -22 LUFS";

    HART_EXPECT_FLOAT_EQ (
        lufs (audio).at (hart::Slice::time (102_s, 108_s)).get (shortTerm()),
        -21_LUFS,
        0.1_LU
        )
        << "LUFS-S at segment 18 == -21 LUFS";

    HART_EXPECT_FLOAT_EQ (
        lufs (audio).at (hart::Slice::time (108_s, 114_s)).get (shortTerm()),
        -20_LUFS,
        0.1_LU
        )
        << "LUFS-S at segment 19 == -20 LUFS";

    HART_EXPECT_FLOAT_EQ (
        lufs (audio).at (hart::Slice::time (114_s, 120_s)).get (shortTerm()),
        -19_LUFS,
        0.1_LU
        )
        << "LUFS-S at segment 20 == -19 LUFS";

}

HART_TEST_WITH_TAGS ("Metrics - LUFS - Satisfies requirements for EBU Tech 3341 test case #12", "[Metrics][LUFS]")
{
    using hart::lufs;
    using hart::LUFS::momentary;

    // Approximates seq-3341-12-24bit.wav from EBU Tech 3341 test case #12.
    // Repeat a 400 ms cycle 25 times: 180 ms at -20 dBFS, then 220 ms at -30 dBFS.
    const auto volumeEnvelope = SegmentedEnvelope (-20_dB)
        .hold (180_ms)
        .rampTo (-30_dB, 0_s);

    AudioBuffer oneCycle = processAudioWith (Bypass())
        .withInputSignal (SineWave (1_kHz) >> GainDb().withEnvelope (GainDb::gainDb, volumeEnvelope))
        .withDuration (400_ms)
        .withSampleRate (48_kHz)
        .inStereo()
        .processAndGetOutputAudio();

    const AudioBuffer audio = processAudioWith (Bypass())
        .withInputSignal (AudioBufferSignal (std::move (oneCycle), hart::Loop::yes))
        .withDuration (10_s)
        .withSampleRate (48_kHz)
        .inStereo()
        .processAndGetOutputAudio();

    // Both extrema must match over the steady portion after the first second.
    const auto query = lufs (audio).at (hart::Slice::time (1_s, 10_s));
    HART_EXPECT_FLOAT_EQ (query.get (momentary (hart::min())), -23_LUFS, 0.1_LU);
    HART_EXPECT_FLOAT_EQ (query.get (momentary()), -23_LUFS, 0.1_LU);
}

HART_PARAMETRISED_TEST_WITH_TAGS ("Metrics - LUFS - Satisfies requirements for EBU Tech 3341 test case #13", "[Metrics][LUFS]")
{
    using hart::lufs;
    using hart::LUFS::momentary;

    // Approximates seq-3341-13-{i + 1}-24bit.wav from EBU Tech 3341 test case #13.
    // The reference archive uses a doubled .wav suffix for files 3 through 20.
    const double i = HART_GENERATE_VALUE (
        0.0, 1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0,
        10.0, 11.0, 12.0, 13.0, 14.0, 15.0, 16.0, 17.0, 18.0, 19.0
        );
    const double silenceDurationSeconds = 20_ms * i;
    const double renderDurationSeconds = silenceDurationSeconds + 400_ms + 1_s;

    const auto volumeEnvelope = SegmentedEnvelope (-oo_dB)
        .hold (silenceDurationSeconds)
        .rampTo (-23_dB, 0_s)
        .hold (400_ms)
        .rampTo (-oo_dB, 0_s);

    const AudioBuffer audio = processAudioWith (Bypass())
        .withInputSignal (SineWave (1_kHz) >> GainDb().withEnvelope (GainDb::gainDb, volumeEnvelope))
        .withDuration (renderDurationSeconds)
        .withSampleRate (48_kHz)
        .inStereo()
        .processAndGetOutputAudio();

    HART_EXPECT_FLOAT_EQ (lufs (audio).get (momentary()), -23_LUFS, 0.1_LU);
}

HART_TEST_WITH_TAGS ("Metrics - LUFS - Explicit temporal reducers differ from the default maximum", "[Metrics][LUFS]")
{
    using hart::lufs;
    using hart::LUFS::shortTerm;
    using hart::LUFS::momentary;

    const auto volumeEnvelope = SegmentedEnvelope (-30_dB)
        .hold (6_s)
        .rampTo (-12_dB, 10_ms)
        .hold (6_s)
        .rampTo (-24_dB, 10_ms);

    const AudioBuffer audio = processAudioWith (Bypass())
        .withInputSignal (SineWave() >> GainDb().withEnvelope (GainDb::gainDb, volumeEnvelope))
        .withDuration (18_s)
        .processAndGetOutputAudio();

    const auto query = lufs (audio).ch (hart::left);

    // The default temporal reducer is max().
    HART_EXPECT_FLOAT_EQ (
        query.get (shortTerm (hart::max())),
        query.get (shortTerm()),
        1e-8
        );

    HART_EXPECT_FLOAT_EQ (
        query.get (momentary (hart::max())),
        query.get (momentary()),
        1e-8
        );

    // The median includes quieter passages and must differ from the loudest window.
    HART_EXPECT_FLOAT_NE (
        query.get (shortTerm (hart::percentile (0.5))),
        query.get (shortTerm()),
        0.1_LU
        );

    HART_EXPECT_FLOAT_NE (
        query.get (momentary (hart::percentile (0.5))),
        query.get (momentary()),
        0.1_LU
        );

}

HART_TEST_WITH_TAGS ("Metrics - LUFS - Mono channel readings reflect the gain difference between left and right", "[Metrics][LUFS]")
{
    using hart::lufs;
    using hart::LUFS::integrated;
    using hart::LUFS::shortTerm;
    using hart::LUFS::momentary;

    const AudioBuffer audio = processAudioWith (GainDb (-3_dB).atChannel (hart::right))
        .withInputSignal (SineWave())
        .withDuration (3_s)
        .withInputChannels (2)
        .withOutputChannels (2)
        .processAndGetOutputAudio();

    const double leftIntegrated = lufs (audio).ch (hart::left).get (integrated());
    const double rightIntegrated = lufs (audio).ch (hart::right).get (integrated());
    const double leftShortTerm = lufs (audio).ch (hart::left).get (shortTerm());
    const double rightShortTerm = lufs (audio).ch (hart::right).get (shortTerm());
    const double leftMomentary = lufs (audio).ch (hart::left).get (momentary());
    const double rightMomentary = lufs (audio).ch (hart::right).get (momentary());

    HART_EXPECT_FLOAT_EQ (leftIntegrated - rightIntegrated, 3_LU, 0.1_LU);
    HART_EXPECT_FLOAT_EQ (leftShortTerm - rightShortTerm, 3_LU, 0.1_LU);
    HART_EXPECT_FLOAT_EQ (leftMomentary - rightMomentary, 3_LU, 0.1_LU);
}

HART_TEST_WITH_TAGS ("Metrics - LUFS - Custom 5.0 positions match the default and change the reading when moved", "[Metrics][LUFS]")
{
    using hart::lufs;
    using hart::LUFS::integrated;
    using hart::LUFS::shortTerm;
    using hart::LUFS::momentary;

    const AudioBuffer audio = processAudioWith (Bypass())
        .withInputSignal (SineWave())
        .withDuration (3_s)
        .withInputChannels (5)
        .withOutputChannels (5)
        .processAndGetOutputAudio();

    const auto query = lufs (audio).ch ({ 0, 1, 2, 3, 4 });

    // Speaker descriptor: { azimuthRadians, elevationRadians, lfe }.
    // HART's _deg literal converts degrees to radians.
    const std::vector<hart::SpeakerPosition> defaultPositions = {
        { 30_deg, 0_deg, hart::LFE::no },   // L
        { -30_deg, 0_deg, hart::LFE::no },  // R
        { 0_deg, 0_deg, hart::LFE::no },    // C
        { 110_deg, 0_deg, hart::LFE::no },  // Ls
        { -110_deg, 0_deg, hart::LFE::no }   // Rs
    };

    // Moving the surrounds to +/-150 degrees removes their +1.5 dB weighting.
    const std::vector<hart::SpeakerPosition> differentPositions = {
        { 30_deg, 0_deg, hart::LFE::no },   // L
        { -30_deg, 0_deg, hart::LFE::no },  // R
        { 0_deg, 0_deg, hart::LFE::no },    // C
        { 150_deg, 0_deg, hart::LFE::no },  // Ls
        { -150_deg, 0_deg, hart::LFE::no }   // Rs
    };

    // The explicit standard positions must match the inferred layout.
    HART_EXPECT_FLOAT_EQ (
        query.get (integrated (defaultPositions)),
        query.get (integrated()),
        1e-8
        );

    HART_EXPECT_FLOAT_EQ (
        query.get (shortTerm (hart::max(), defaultPositions)),
        query.get (shortTerm()),
        1e-8
        );

    HART_EXPECT_FLOAT_EQ (
        query.get (momentary (hart::max(), defaultPositions)),
        query.get (momentary()),
        1e-8
        );

    // Different speaker positions must affect the programme loudness.
    HART_EXPECT_FLOAT_NE (
        query.get (integrated (differentPositions)),
        query.get (integrated()),
        0.1_LU
        );

    HART_EXPECT_FLOAT_NE (
        query.get (shortTerm (hart::max(), differentPositions)),
        query.get (shortTerm()),
        0.1_LU
        );

    HART_EXPECT_FLOAT_NE (
        query.get (momentary (hart::max(), differentPositions)),
        query.get (momentary()),
        0.1_LU
        );

}

HART_TEST_WITH_TAGS ("Metrics - LUFS - Custom 5.1 positions match the default and change the reading when moved", "[Metrics][LUFS]")
{
    using hart::lufs;
    using hart::LUFS::integrated;
    using hart::LUFS::shortTerm;
    using hart::LUFS::momentary;

    const AudioBuffer audio = processAudioWith (Bypass())
        .withInputSignal (SineWave())
        .withDuration (3_s)
        .withInputChannels (6)
        .withOutputChannels (6)
        .processAndGetOutputAudio();

    const auto query = lufs (audio).ch ({ 0, 1, 2, 3, 4, 5 });

    // Speaker descriptor: { azimuthRadians, elevationRadians, lfe }.
    // HART's _deg literal converts degrees to radians.
    const std::vector<hart::SpeakerPosition> defaultPositions = {
        { 30_deg, 0_deg, hart::LFE::no },   // L
        { -30_deg, 0_deg, hart::LFE::no },  // R
        { 0_deg, 0_deg, hart::LFE::no },    // C
        { 0_deg, 0_deg, hart::LFE::yes },   // LFE
        { 110_deg, 0_deg, hart::LFE::no },  // Ls
        { -110_deg, 0_deg, hart::LFE::no }  // Rs
    };

    // Moving the surrounds to +/-150 degrees removes their +1.5 dB weighting.
    const std::vector<hart::SpeakerPosition> differentPositions = {
        { 30_deg, 0_deg, hart::LFE::no },   // L
        { -30_deg, 0_deg, hart::LFE::no },  // R
        { 0_deg, 0_deg, hart::LFE::no },    // C
        { 0_deg, 0_deg, hart::LFE::yes },   // LFE
        { 150_deg, 0_deg, hart::LFE::no },  // Ls
        { -150_deg, 0_deg, hart::LFE::no }  // Rs
    };

    // The explicit standard positions must match the inferred layout.
    HART_EXPECT_FLOAT_EQ (
        query.get (integrated (defaultPositions)),
        query.get (integrated()),
        1e-8
        );

    HART_EXPECT_FLOAT_EQ (
        query.get (shortTerm (hart::max(), defaultPositions)),
        query.get (shortTerm()),
        1e-8
        );

    HART_EXPECT_FLOAT_EQ (
        query.get (momentary (hart::max(), defaultPositions)),
        query.get (momentary()),
        1e-8
        );

    // Different speaker positions must affect the programme loudness.
    HART_EXPECT_FLOAT_NE (
        query.get (integrated (differentPositions)),
        query.get (integrated()),
        0.1_LU
        );

    HART_EXPECT_FLOAT_NE (
        query.get (shortTerm (hart::max(), differentPositions)),
        query.get (shortTerm()),
        0.1_LU
        );

    HART_EXPECT_FLOAT_NE (
        query.get (momentary (hart::max(), differentPositions)),
        query.get (momentary()),
        0.1_LU
        );

}

HART_TEST_WITH_TAGS ("Metrics - LUFS - Skipping the LFE channel of 5.1 audio matches the 5.0 reading", "[Metrics][LUFS]")
{
    using hart::lufs;
    using hart::LUFS::integrated;
    using hart::LUFS::shortTerm;
    using hart::LUFS::momentary;

    const AudioBuffer audio = processAudioWith (Bypass())
        .withInputSignal (SineWave())
        .withDuration (3_s)
        .withInputChannels (6)
        .withOutputChannels (6)
        .processAndGetOutputAudio();

    const auto query = lufs (audio).ch ({ 0, 1, 2, 3, 4, 5 });

    // Generated 5.1 order is L/R/C/LFE/Ls/Rs. LFE contains a tone but must be excluded.
    // Selecting L/R/C/Ls/Rs leaves the same programme represented as 5.0.
    HART_EXPECT_FLOAT_EQ (
        query.ch ({ 0, 1, 2, 4, 5 }).get (integrated()),
        query.get (integrated()),
        1e-8
        );

    HART_EXPECT_FLOAT_EQ (
        query.ch ({ 0, 1, 2, 4, 5 }).get (shortTerm()),
        query.get (shortTerm()),
        1e-8
        );

    HART_EXPECT_FLOAT_EQ (
        query.ch ({ 0, 1, 2, 4, 5 }).get (momentary()),
        query.get (momentary()),
        1e-8
        );

}

HART_TEST_WITH_TAGS ("Metrics - LUFS - Default and native units return LUFS", "[Metrics][LUFS][Units]")
{
    using hart::lufs;
    using hart::LUFS::integrated;

    const AudioBuffer audio = processAudioWith (Bypass())
        .withInputSignal (SineWave())
        .processAndGetOutputAudio();

    const auto query = lufs (audio).ch (hart::left);

    HART_EXPECT_FLOAT_EQ (query.get (integrated()), query.as (LUFS).get (integrated()), 1e-8);
    HART_EXPECT_FLOAT_EQ (query.as (native).get (integrated()), query.get (integrated()), 1e-8);
}

HART_TEST_WITH_TAGS ("Metrics - LUFS - Supports LU unit", "[Metrics][LUFS][Units]")
{
    using hart::lufs;
    using hart::LUFS::integrated;

    const AudioBuffer audio = processAudioWith (Bypass())
        .withInputSignal (SineWave())
        .processAndGetOutputAudio();

    const auto query = lufs (audio).ch (hart::left);

    HART_EXPECT_FLOAT_NE (
        query.as (LU).get (integrated()),
        query.as (LUFS).get (integrated()),
        0.1_LU
        );

}

HART_TEST_WITH_TAGS ("Metrics - LUFS - LU readings are 23 LU above LUFS readings", "[Metrics][LUFS][Units]")
{
    using hart::lufs;
    using hart::LUFS::integrated;

    const AudioBuffer audio = processAudioWith (Bypass())
        .withInputSignal (SineWave())
        .processAndGetOutputAudio();

    const auto query = lufs (audio).ch (hart::left);

    const double relativeLoudness = query.as (LU).get (integrated());
    const double absoluteLoudness = query.as (LUFS).get (integrated());

    HART_EXPECT_FLOAT_EQ (relativeLoudness - absoluteLoudness, 23_LU, 1e-8);
}

HART_PARAMETRISED_TEST_WITH_TAGS ("Metrics - LUFS - Short clips integrate the available audio without complete temporal windows", "[Metrics][LUFS]")
{
    using hart::lufs;
    using hart::LUFS::integrated;
    using hart::LUFS::shortTerm;
    using hart::LUFS::momentary;

    const double duration = HART_GENERATE_VALUE (100_ms, 300_ms);
    const AudioBuffer audio = processAudioWith (Bypass())
        .withInputSignal (SineWave())
        .withDuration (duration)
        .processAndGetOutputAudio();

    const auto query = lufs (audio).ch (hart::left);

    HART_EXPECT_NOT_NAN (query.get (integrated()));
    HART_EXPECT_IS_NAN (query.get (shortTerm()));
    HART_EXPECT_IS_NAN (query.get (momentary()));
}

HART_TEST_WITH_TAGS ("Metrics - LUFS - An empty analysis range returns undefined readings", "[Metrics][LUFS]")
{
    using hart::lufs;
    using hart::LUFS::integrated;
    using hart::LUFS::shortTerm;
    using hart::LUFS::momentary;

    const AudioBuffer audio = processAudioWith (Bypass())
        .withInputSignal (SineWave())
        .processAndGetOutputAudio();

    const auto query = lufs (audio).ch (hart::left)
        .at (hart::Slice::frames (audio.getNumFrames(), audio.getNumFrames() + 1));

    HART_EXPECT_IS_NAN (query.get (integrated()));
    HART_EXPECT_IS_NAN (query.get (shortTerm()));
    HART_EXPECT_IS_NAN (query.get (momentary()));
}

HART_TEST_WITH_TAGS ("Metrics - PLR - Mono 1 kHz sine reads approximately 3 dB", "[Metrics][PLR]")
{
    using hart::plr;
    using hart::PLR::reduce;

    const AudioBuffer audio = processAudioWith (Bypass())
        .withInputSignal (SineWave (1_kHz) >> GainDb (-20_dB))
        .withDuration (3_s)
        .inMono()
        .processAndGetOutputAudio();

    HART_EXPECT_FLOAT_EQ (plr (audio).get (reduce()), 3_dB, 0.2_dB);
}

HART_TEST_WITH_TAGS ("Metrics - PLR - Uniform attenuation preserves PLR", "[Metrics][PLR]")
{
    using hart::plr;
    using hart::PLR::reduce;

    // Both levels stay above the absolute loudness gate, without clipping.
    const AudioBuffer audio = processAudioWith (Bypass())
        .withInputSignal (SineWave (1_kHz) >> GainDb (-20_dB))
        .withDuration (3_s)
        .inMono()
        .processAndGetOutputAudio();

    const AudioBuffer quieterAudio = processAudioWith (GainDb (-6_dB))
        .withInputSignal (AudioBufferSignal (audio))
        .withDuration (3_s)
        .inMono()
        .processAndGetOutputAudio();

    HART_EXPECT_FLOAT_EQ (
        plr (quieterAudio).get (reduce()),
        plr (audio).get (reduce()),
        0.01_dB
        );

}

HART_TEST_WITH_TAGS ("Metrics - PLR - Stereo uses programme loudness instead of per-channel PLRs", "[Metrics][PLR]")
{
    using hart::plr;
    using hart::PLR::reduce;

    const AudioBuffer audio = processAudioWith (Bypass())
        .withInputSignal (SineWave (1_kHz) >> GainDb (-20_dB))
        .withDuration (3_s)
        .inStereo()
        .processAndGetOutputAudio();

    HART_EXPECT_FLOAT_EQ (plr (audio).get (reduce()), 0_dB, 0.2_dB);
    HART_EXPECT_FLOAT_EQ (plr (audio).ch (hart::left).get (reduce()), 3_dB, 0.2_dB);
}

HART_TEST_WITH_TAGS ("Metrics - PLR - LFE contributes to true peak but not programme loudness", "[Metrics][PLR]")
{
    using hart::plr;
    using hart::PLR::reduce;

    // L/R/C/LFE/Ls/Rs: LFE is 20 dB louder than all other channels.
    const AudioBuffer audio = processAudioWith (Bypass())
        .withInputSignal (SineWave (1_kHz) >> GainDb (-20_dB).atChannels ({ 0, 1, 2, 4, 5 }))
        .withDuration (3_s)
        .withInputChannels (6)
        .withOutputChannels (6)
        .processAndGetOutputAudio();

    const double withLfe = plr (audio).get (reduce());
    const double withoutLfe = plr (audio).ch ({ 0, 1, 2, 4, 5 }).get (reduce());
    HART_EXPECT_FLOAT_EQ (withLfe - withoutLfe, 20_dB, 0.01_dB);
}

HART_TEST_WITH_TAGS ("Metrics - PLR - Silence has no defined peak-to-loudness ratio", "[Metrics][PLR]")
{
    using hart::plr;
    using hart::PLR::reduce;

    const AudioBuffer audio = processAudioWith (Bypass())
        .withInputSignal (Silence())
        .inMono()
        .processAndGetOutputAudio();

    HART_EXPECT_IS_NAN (plr (audio).get (reduce()));
}

HART_TEST_WITH_TAGS ("Metrics - PLR - Default unit is dB", "[Metrics][PLR][Unit]")
{
    using hart::plr;
    using hart::PLR::reduce;

    const AudioBuffer audio = processAudioWith (Bypass())
        .withInputSignal (WhiteNoise())
        .processAndGetOutputAudio();

    const double plrDefault = plr (audio).ch (hart::left).get (reduce());
    const double plrNative = plr (audio).ch (hart::left).as (native).get (reduce());
    const double plrDb = plr (audio).ch (hart::left).as (dB).get (reduce());

    HART_EXPECT_FLOAT_EQ (plrDefault, plrDb, 1e-8);
    HART_EXPECT_FLOAT_EQ (plrNative, plrDb, 1e-8);
}
