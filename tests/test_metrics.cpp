#include <algorithm>  // max()
#include <cmath>  // abs(), isnan(), isinf(), cos(), acos()

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

        .expectTrue ([] (const AudioBuffer& output) { return HART_FLOAT_EQ (samplePeak (output).as (dB).get (mean()), expectedDbMean, 1e-2); }, "Sample Peak in dB, mean() reducer")
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

HART_TEST ("Metrics - Loudest Bin Magnitude")
{
    using AudioBuffer = hart::AudioBuffer<float>;
    using Spectrum = hart::Spectrum;
    using Channel = hart::Channel;
    using hart::loudestBinMagnitude;
    using hart::ratioToDecibels;

    processAudioWith (GainDb (0_dB))
        .withLabel ("Silence")
        .withInputSignal (Silence())
        .inMono()
        .expectTrue ([] (const AudioBuffer& output) { return HART_FLOAT_EQ (loudestBinMagnitude (Spectrum (output)).as (linear).get(), 0.0, 1e-8); }, "The loudest bin is zero")
        .process();

    std::array<std::pair<double, double>, 3> testFrequencyPairs = {{
        {1_kHz, 500_Hz},
        {123_Hz, 456_Hz},
        {10_Hz, 18_kHz}
    }};

    for (std::pair<double, double> frequencies : testFrequencyPairs)
    {
        const double loudFrequencyHz = frequencies.first;
        const double quietFrequencyHz = frequencies.second;

        AudioBuffer output;
        processAudioWith (GainDb (0_dB))
            .withLabel (HART_STR ("Two sine waves: " << loudFrequencyHz << " Hz and " << quietFrequencyHz << " Hz"))
            .withInputSignal (SineWave (loudFrequencyHz) + (SineWave (quietFrequencyHz) >> GainDb (-3_dB)))
            .inMono()
            .saveOutputTo (output)
            .process();

        const Spectrum spectrum (output);
        const double measuredMagnitudeLinear = loudestBinMagnitude (spectrum).as (linear);
        const double expectedMagnitudeLinear = spectrum.getBinMagnitude (Channel::left, loudFrequencyHz);

        HART_EXPECT_FLOAT_EQ (measuredMagnitudeLinear, expectedMagnitudeLinear, 1e-8) << "Bin magnitude at correct frequency";
        HART_EXPECT_GT (measuredMagnitudeLinear, spectrum.getBinMagnitude (Channel::left, quietFrequencyHz)) << "Louder than other sine wave";

        HART_EXPECT_FLOAT_EQ (measuredMagnitudeLinear, loudestBinMagnitude (spectrum).get(), 1e-8) << "Implicit unit";
        HART_EXPECT_FLOAT_EQ (measuredMagnitudeLinear, loudestBinMagnitude (spectrum).as (native).get(), 1e-8) << "Native unit";
        HART_EXPECT_FLOAT_EQ (ratioToDecibels (measuredMagnitudeLinear), loudestBinMagnitude (spectrum).as (dB).get(), 1e-8) << "Decibels";
    }
}

HART_TEST ("Metrics - Quinn's Second Estimator")
{
    using Spectrum = hart::Spectrum;
    using hart::quinns2;
    using std::pow;

    const std::array<double, 5> expectedFundamentalsHz ({123_Hz, 456_Hz, 1_kHz, 5_kHz, 15_kHz});

    for (const double expectedFundamentalHz : expectedFundamentalsHz)
    {
        AudioBuffer output;
        processAudioWith (GainDb (0_dB))
            .withInputSignal (SineWave (expectedFundamentalHz))
            .inMono()
            .saveOutputTo (output)
            .process();

        const Spectrum spectrum (output);
        const double estimatedFundamentalHz = quinns2 (spectrum);

        HART_EXPECT_FREQ_EQ (estimatedFundamentalHz, expectedFundamentalHz, 15_cents)
            << "Sine wave at " << expectedFundamentalHz << " Hz";
    }

    for (const double expectedFundamentalHz : expectedFundamentalsHz)
    {
        AudioBuffer output;
        processAudioWith (GainDb (0_dB))
            .withInputSignal (Sawtooth (expectedFundamentalHz))
            .inMono()
            .saveOutputTo (output)
            .process();

        const Spectrum spectrum (output);
        const double estimatedFundamentalHz = quinns2 (spectrum);

        HART_EXPECT_FREQ_EQ (estimatedFundamentalHz, expectedFundamentalHz, 15_cents)
            << "Sawtooth at " << expectedFundamentalHz << " Hz";
    }
}

HART_TEST ("Metrics - Loudest Bin Frequency")
{
    using AudioBuffer = hart::AudioBuffer<float>;
    using Spectrum = hart::Spectrum;
    using hart::loudestBinFrequency;
    using std::pow;

    const std::array<double, 5> expectedFundamentalsHz ({123_Hz, 456_Hz, 1_kHz, 5_kHz, 15_kHz});

    for (const double expectedFundamentalHz : expectedFundamentalsHz)
    {
        AudioBuffer output;
        processAudioWith (GainDb (0_dB))
            .withInputSignal (SineWave (expectedFundamentalHz))
            .inMono()
            .saveOutputTo (output)
            .process();

        const Spectrum spectrum (output);
        const double estimatedFundamentalHz = loudestBinFrequency (spectrum);

        const size_t expectedBinIndex = spectrum.findClosestBin (expectedFundamentalHz);
        const double expectedBinFrequency = spectrum.getBinFrequencyHz (expectedBinIndex);
        const double binHalfWidthHz = 0.5 * spectrum.getBinWidthHz();
        const double expectedFundamentalLowerHz = expectedBinFrequency - binHalfWidthHz;
        const double expectedFundamentalUpperHz = expectedBinFrequency + binHalfWidthHz;

        HART_EXPECT_FLOAT_IN_RANGE (estimatedFundamentalHz, expectedFundamentalLowerHz, expectedFundamentalUpperHz, 1e-8)
            << "Sine wave at " << expectedFundamentalHz << " Hz";
    }

    for (const double expectedFundamentalHz : expectedFundamentalsHz)
    {
        AudioBuffer output;
        processAudioWith (GainDb (0_dB))
            .withInputSignal (Sawtooth (expectedFundamentalHz))
            .inMono()
            .saveOutputTo (output)
            .process();

        const Spectrum spectrum (output);
        const double estimatedFundamentalHz = loudestBinFrequency (spectrum);

        const size_t expectedBinIndex = spectrum.findClosestBin (expectedFundamentalHz);
        const double expectedBinFrequency = spectrum.getBinFrequencyHz (expectedBinIndex);
        const double binHalfWidthHz = 0.5 * spectrum.getBinWidthHz();
        const double expectedFundamentalLowerHz = expectedBinFrequency - binHalfWidthHz;
        const double expectedFundamentalUpperHz = expectedBinFrequency + binHalfWidthHz;

        HART_EXPECT_FLOAT_IN_RANGE (estimatedFundamentalHz, expectedFundamentalLowerHz, expectedFundamentalUpperHz, 1e-8)
            << "Sawtooth at " << expectedFundamentalHz << " Hz";
    }
}

HART_TEST ("Metrics - Interpolated Peak Frequency")
{
    using Spectrum = hart::Spectrum;
    using hart::quinns2;
    using std::pow;

    const std::array<double, 5> expectedFundamentalsHz ({123_Hz, 456_Hz, 1_kHz, 5_kHz, 15_kHz});

    for (const double expectedFundamentalHz : expectedFundamentalsHz)
    {
        AudioBuffer output;
        processAudioWith (GainDb (0_dB))
            .withInputSignal (SineWave (expectedFundamentalHz))
            .inMono()
            .saveOutputTo (output)
            .process();

        const Spectrum spectrum (output);
        const double estimatedFundamentalHz = interpolatedPeakFrequency (spectrum);

        HART_EXPECT_FREQ_EQ (estimatedFundamentalHz, expectedFundamentalHz, 15_cents)
            << "Sine wave at " << expectedFundamentalHz << " Hz";
    }

    for (const double expectedFundamentalHz : expectedFundamentalsHz)
    {
        AudioBuffer output;
        processAudioWith (GainDb (0_dB))
            .withInputSignal (Sawtooth (expectedFundamentalHz))
            .inMono()
            .saveOutputTo (output)
            .process();

        const Spectrum spectrum (output);
        const double estimatedFundamentalHz = interpolatedPeakFrequency (spectrum);

        HART_EXPECT_FREQ_EQ (estimatedFundamentalHz, expectedFundamentalHz, 15_cents)
            << "Sawtooth at " << expectedFundamentalHz << " Hz";
    }
}

HART_TEST ("Metrics - Quinn's Second Estimator vs Loudest Bin Frequency - DC")
{
    using AudioBuffer = hart::AudioBuffer<float>;
    using Spectrum = hart::Spectrum;
    using hart::loudestBinFrequency;
    using hart::quinns2;
    using std::pow;

    auto dcSignal = SignalFunction (
        [] (AudioBuffer& buffer)
        {
            buffer.setNumFrames (1);

            for (size_t channel = 0; channel < buffer.getNumChannels(); ++channel)
                buffer[channel][0] = 0.1f;
        },
        "DC signal at -20 dB"
        );

    AudioBuffer output;
    processAudioWith (GainDb (0_dB))
        .withInputSignal (std::move (dcSignal))
        .inMono()
        .saveOutputTo (output)
        .assertTrue (PeaksAt (-20_dB))
        .process();

    const Spectrum dcSpectrum (output);
    const double estimatedFundamentalHzA = loudestBinFrequency (dcSpectrum);

    const size_t expectedBinIndex = dcSpectrum.findClosestBin (0_Hz);
    const double expectedBinFrequency = dcSpectrum.getBinFrequencyHz (expectedBinIndex);
    const double binHalfWidthHz = 0.5 * dcSpectrum.getBinWidthHz();
    const double expectedFundamentalLowerHz = expectedBinFrequency - binHalfWidthHz;
    const double expectedFundamentalUpperHz = expectedBinFrequency + binHalfWidthHz;

    HART_EXPECT_FLOAT_IN_RANGE (estimatedFundamentalHzA, expectedFundamentalLowerHz, expectedFundamentalUpperHz, 1e-8)
        << "loudestBinFrequency() works correctly at near DC frequencies";

    const double estimatedFundamentalHzB = quinns2 (dcSpectrum);
    HART_EXPECT_TRUE (std::isnan (estimatedFundamentalHzB))
        << "quinns2() is undefined at near DC frequencies";
}

HART_TEST ("Metrics - Quinn's Second Estimator vs Loudest Bin Frequency - Nyquist Signal")
{
    using AudioBuffer = hart::AudioBuffer<float>;
    using Spectrum = hart::Spectrum;
    using hart::loudestBinFrequency;
    using hart::quinns2;
    using std::pow;

    auto nyquistSignal = SignalFunction (
        [] (AudioBuffer& buffer)
        {
            buffer.setNumFrames (2);

            for (size_t channel = 0; channel < buffer.getNumChannels(); ++channel)
            {
                buffer[channel][0] = 1.0;
                buffer[channel][1] = -1.0;
            }
        },
        "Nyquist signal at unity gain"
        );

    AudioBuffer output;
    processAudioWith (GainDb (0_dB))
        .withInputSignal (std::move (nyquistSignal))
        .inMono()
        .saveOutputTo (output)
        .assertTrue (PeaksAt (0_dB))
        .process();

    const Spectrum dcSpectrum (output);
    const double estimatedFundamentalHzA = loudestBinFrequency (dcSpectrum);

    const double nyquistFrequencyHz = 0.5 * hart::CLIConfig::getInstance().getDefaultSampleRateHz();
    const size_t expectedBinIndex = dcSpectrum.findClosestBin (nyquistFrequencyHz);
    const double expectedBinFrequency = dcSpectrum.getBinFrequencyHz (expectedBinIndex);
    const double binHalfWidthHz = 0.5 * dcSpectrum.getBinWidthHz();
    const double expectedFundamentalLowerHz = expectedBinFrequency - binHalfWidthHz;
    const double expectedFundamentalUpperHz = expectedBinFrequency + binHalfWidthHz;

    HART_EXPECT_FLOAT_IN_RANGE (estimatedFundamentalHzA, expectedFundamentalLowerHz, expectedFundamentalUpperHz, 1e-8)
        << "loudestBinFrequency() works correctly near Nyquist frequency";

    const double estimatedFundamentalHzB = quinns2 (dcSpectrum);
    HART_EXPECT_TRUE (std::isnan (estimatedFundamentalHzB))
        << "quinns2() is undefined near Nyquist frequency";
}

HART_TEST ("Metrics - Spectral Centroid - Mock Spectra")
{
    using Spectrum = hart::Spectrum;
    using hart::spectralCentroid;
    using hart::loudestBinFrequency;
    using hart::roundToSizeT;
    using hart::nth;

    auto& cliConfig = hart::CLIConfig::getInstance();
    const double sampleRateHz = cliConfig.getDefaultSampleRateHz();
    const double signalDurationSeconds = cliConfig.getDefaultRenderDurationSeconds();
    const size_t signalDurationFrames = roundToSizeT (signalDurationSeconds * sampleRateHz);
    constexpr int numChannels = 10;

    auto mockSpectrum = Spectrum::zeros (numChannels, signalDurationFrames, sampleRateHz);
    const size_t numBins = mockSpectrum.getNumBins();

    // Single frequency
    for (size_t channel = 0; channel < numChannels / 2; ++channel)
    {
        const size_t bin = (10 + channel * 100) % numBins;
        mockSpectrum[channel][bin] = std::complex<double> (1.0, 0.0);

        const double expectedCentroidHz = mockSpectrum.getBinFrequencyHz (bin);

        HART_EXPECT_FLOAT_EQ (
            spectralCentroid (mockSpectrum).get (nth (channel)),
            expectedCentroidHz,
            1e-12
        )
        << "Single-frequency centroid at channel " << channel;
    }

    for (size_t channel = 5; channel < 10; ++channel)
    {
        const size_t binA = (50 + (channel - 5) * 40) % numBins;
        const size_t binB = (binA + (channel - 5) * 100) % numBins;

        constexpr double magnitudeA = 1.0;
        constexpr double magnitudeB = 3.0;

        mockSpectrum[channel][binA] = std::complex<double> (magnitudeA, 0.0);
        mockSpectrum[channel][binB] = std::complex<double> (magnitudeB, 0.0);

        const double freqA = mockSpectrum.getBinFrequencyHz (binA);
        const double freqB = mockSpectrum.getBinFrequencyHz (binB);

        const double expectedCentroidHz =
            (freqA * magnitudeA + freqB * magnitudeB)
            / (magnitudeA + magnitudeB);

        HART_EXPECT_FLOAT_EQ (
            spectralCentroid (mockSpectrum).get (nth (channel)),
            expectedCentroidHz,
            1e-12
        )
            << "Dual frequency centroid at channel " << channel;
    }
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
        .expectTrue ([] (const AudioBuffer& buffer) { return HART_FLOAT_EQ (rms (buffer).get(), invSqrt2, 1e-6); }, "RMS = 1 / sqrt(2)")
        .process();

    processAudioWith (GainDb (0_dB))
        .withLabel ("Sine sweep")
        .withInputSignal (SineSweep ())
        .inMono()
        .expectTrue ([] (const AudioBuffer& buffer) { return HART_FLOAT_EQ (rms (buffer).get(), invSqrt2, 0.01); }, "RMS ~= 1 / sqrt(2)")
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
        .expectTrue ([] (const AudioBuffer& buffer) { return HART_FLOAT_EQ (rms (buffer).get(), invSqrt3, 0.01); }, "RMS ~= 1 / sqrt(3)")
        .process();
}

HART_TEST ("Metrics - RMS for Spectrum - Matches RMS for AudioBuffer")
{
    using AnalysisContext = hart::AnalysisContext<float>;
    using hart::rms;
    using hart::nextPowerOfTwoDuration;
    using hart::CLIConfig;

    const double renderDuration = nextPowerOfTwoDuration (CLIConfig::getInstance().getDefaultRenderDurationSeconds());

    processAudioWith (Bypass())
        .withLabel ("Sine wave")
        .withInputSignal (SineWave())
        .withDuration (renderDuration)
        .inMono()
        .expectTrue ([] (AnalysisContext ac) { return HART_FLOAT_EQ (rms (ac.outputAudio()).get(), rms (ac.outputSpectrum()).get(), 1e-6); }, "RMS in time domain = RMS of spectrum")
        .process();

    processAudioWith (Bypass())
        .withLabel ("White noise")
        .withInputSignal (WhiteNoise())
        .withDuration (renderDuration)
        .inMono()
        .expectTrue ([] (AnalysisContext ac) { return HART_FLOAT_EQ (rms (ac.outputAudio()).get(), rms (ac.outputSpectrum()).get(), 1e-6); }, "RMS in time domain = RMS of spectrum")
        .process();
}

HART_TEST ("Metrics - RMS for Spectrum - Units")
{
    using AudioBuffer = hart::AudioBuffer<float>;
    using hart::Spectrum;
    using hart::rms;
    using hart::ratioToDecibels;

    AudioBuffer audioBuffer;
    processAudioWith (Bypass())
        .withInputSignal (WhiteNoise())
        .saveOutputTo (audioBuffer)
        .inMono()
        .process();

    Spectrum spectrum (audioBuffer);
    const double rmsNative = rms (spectrum);
    const double rmsLinear = rms (spectrum).as (linear);
    const double rmsDb = rms (spectrum).as (dB);

    HART_EXPECT_FLOAT_EQ (rmsNative, rmsLinear, 1e-16) << "Linear is default (native)";
    HART_EXPECT_FLOAT_NE (rmsDb, rmsLinear, 1e-16) << "dB value is different from linear one";
    HART_EXPECT_FLOAT_EQ (rmsDb, ratioToDecibels (rmsLinear), 1e-16) << "dB is calculated as voltage, not power";
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

HART_TEST ("Metrics - Spectral Flatness")
{
    using AudioBuffer = hart::AudioBuffer<float>;
    using Spectrum = hart::Spectrum;
    using hart::spectralFlatness;

    processAudioWith (GainDb (0_dB))
        .withLabel ("Pure tone")    
        .withInputSignal (SineWave())
        .expectTrue (
            [] (const AudioBuffer& output)
                { return HART_FLOAT_IN_RANGE (spectralFlatness (Spectrum (output)).get(), 0.0, 0.1, 1e-6); },
            "0.0 <= Spectral Flatness <= 0.1"
            )
        .process();

    processAudioWith (GainDb (0_dB))
        .withLabel ("Not-so-pure tone")    
        .withInputSignal (SineWave() >> GainDb (40_dB) >> HardClip())
        .expectTrue (
            [] (const AudioBuffer& output)
                { return HART_FLOAT_IN_RANGE (spectralFlatness (Spectrum (output)).get(), 0.1, 1.0, 1e-6); },
            "0.1 <= Spectral Flatness <= 1.0"
            )
        .process();

    processAudioWith (GainDb (0_dB))
        .withLabel ("Spiky spectrum")    
        .withInputSignal (Sawtooth())
        .expectTrue (
            [] (const AudioBuffer& output)
                { return HART_FLOAT_IN_RANGE (spectralFlatness (Spectrum (output)).get(), 0.0, 0.2, 1e-6); },
            "0.0 <= Spectral Flatness <= 0.2"
            )
        .process();

    processAudioWith (GainDb (0_dB))
        .withLabel ("White noise")    
        .withInputSignal (WhiteNoise())
        .expectTrue (
            [] (const AudioBuffer& output)
                { return HART_FLOAT_IN_RANGE (spectralFlatness (Spectrum (output)).get(), 0.8, 1.0, 1e-6); },
            "0.8 <= Spectral Flatness <= 1.0"
            )
        .process();

    processAudioWith (GainDb (0_dB))
        .withLabel ("Silence")    
        .withInputSignal (Silence())
        .expectTrue (
            [] (const AudioBuffer& output)
                { return HART_TRUE (std::isnan (spectralFlatness (Spectrum (output)).get())); },
            "Spectral Flatness is NaN"
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

HART_TEST ("Metrics - THD - Regular use cases")
{
    using AudioBuffer = hart::AudioBuffer<float>;
    using AnalysisContext = hart::AnalysisContext<float>;
    using hart::thd;

    for (const double frequencyHz : {50_Hz, 440_Hz, 1000_Hz, 3456.78_Hz})
    {
        const hart::THD::ExperimentSetup setup = hart::THD::ExperimentSetupTuner()
            .withFrequency (frequencyHz)
            .tune();

        processAudioWith (GainDb (0_dB))
            .withLabel (HART_STR ("Pure sine at " << setup.frequencyHz << " Hz"))
            .withInputSignal (SineWave (setup.frequencyHz))
            .withDuration (setup.durationSeconds)
            .assertTrue ([] (const AudioBuffer& output) { return HART_TRUE (hart::isPowerOfTwo (output.getNumFrames())); }, "Rendered audio size in frames is a power of two")
            .expectTrue ([setup] (const AnalysisContext& context) { return HART_FLOAT_EQ (hart::thd (context.outputSpectrum(), setup).get(), 0.0, 1e-8); }, "THD ~= 0")
            .process();

        processAudioWith (HardClip (-0.1_dB))
            .withLabel (HART_STR ("A little distortion, input frequency " << setup.frequencyHz << " Hz"))
            .withInputSignal (SineWave (setup.frequencyHz))
            .withDuration (setup.durationSeconds)
            .assertTrue ([] (const AudioBuffer& output) { return HART_TRUE (hart::isPowerOfTwo (output.getNumFrames())); }, "Rendered audio size in frames is a power of two")
            .expectTrue ([setup] (const AnalysisContext& context) { return HART_FLOAT_IN_RANGE (hart::thd (context.outputSpectrum(), setup).get(), 0.002, 0.003, 1.0e-8); }, "0.002 <= THD <= 0.003")
            .process();

        processAudioWith (HardClip (-6_dB))
            .withLabel (HART_STR ("Heavier distortion, input frequency " << setup.frequencyHz << " Hz"))
            .withInputSignal (SineWave (setup.frequencyHz))
            .withDuration (setup.durationSeconds)
            .assertTrue ([] (const AudioBuffer& output) { return HART_TRUE (hart::isPowerOfTwo (output.getNumFrames())); }, "Rendered audio size in frames is a power of two")
            .expectTrue ([setup] (const AnalysisContext& context) { return HART_FLOAT_IN_RANGE (hart::thd (context.outputSpectrum(), setup).get(), 0.2, 0.3, 1.0e-8); }, "0.2 <= THD <= 0.3")
            .process();
    }
}

HART_TEST ("Metrics - THD - Units")
{
    using AudioBuffer = hart::AudioBuffer<float>;
    using Spectrum = hart::Spectrum;
    using hart::thd;
    using hart::ratioToDecibels;

    const hart::THD::ExperimentSetup setup = hart::THD::ExperimentSetupTuner()
        .withFrequency (1_kHz)
        .tune();

    AudioBuffer outputAudio;
    processAudioWith (HardClip (-1_dB))
        .withInputSignal (SineWave (setup.frequencyHz))
        .withDuration (setup.durationSeconds)
        .saveOutputTo (outputAudio)
        .process();

    const Spectrum outputSpectrum (outputAudio);
    const double thdNative = thd (outputSpectrum, setup);
    const double thdRatio = thd (outputSpectrum, setup).as (ratio);
    const double thdDb = thd (outputSpectrum, setup).as (dB);
    const double thdPercent = thd (outputSpectrum, setup).as (percent);

    HART_EXPECT_FLOAT_EQ (thdNative, thdRatio, 1e-8) << "Ratio is default (native) unit";
    HART_EXPECT_FLOAT_EQ (thdDb, ratioToDecibels (thdRatio), 1e-8) << "Decibels unit";
    HART_EXPECT_FLOAT_EQ (thdPercent, 100 * thdRatio, 1e-8) << "Percentage unit";
}

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

HART_TEST ("Metrics - Spectral Log-Log Slope - Coloured noise")
{
    using AnalysisContext = hart::AnalysisContext<float>;
    using hart::Spectrum;
    using hart::Slice;
    using hart::spectralLogLogSlope;

    const hart::CLIConfig& cliConfig = hart::CLIConfig::getInstance();
    
    // We want a long enough time to get a statistically significant reading, and also
    // at power-of-2 length, so that it's not getting padded or truncated during FFT/IFFT
    const double minRenderDurationSeconds = 1_s;
    const double sampleRateHz = cliConfig.getDefaultSampleRateHz();
    const size_t minRenderDurationFrames = hart::roundToSizeT (minRenderDurationSeconds * sampleRateHz);
    const size_t renderDurationFrames = hart::nextPowerOfTwo (minRenderDurationFrames);
    const double renderDurationSeconds = static_cast<double> (renderDurationFrames) / sampleRateHz;

    processAudioWith (Bypass())
        .withLabel ("Pink Noise")
        .withInputSignal (Spectrum::colouredNoise (Spectrum::ColouredNoise::pink().withDuration (renderDurationFrames)).toSignal<float>())
        .withDuration (renderDurationSeconds)
        .expectTrue (
            [] (AnalysisContext context)
            {
                return HART_FLOAT_EQ (spectralLogLogSlope (context.outputSpectrum()).at (Slice::freq (20_Hz, 20_kHz)).get(), -1.0, 0.005);
            },
            "Slope ~= -1"
            )
        .process();

    processAudioWith (Bypass())
        .withLabel ("White Noise generated in frequency domain")
        .withInputSignal (Spectrum::colouredNoise (Spectrum::ColouredNoise::white().withDuration (renderDurationFrames)).toSignal<float>())
        .withDuration (renderDurationSeconds)
        .expectTrue (
            [] (AnalysisContext context)
            {
                return HART_FLOAT_EQ (spectralLogLogSlope (context.outputSpectrum()).at (Slice::freq (20_Hz, 20_kHz)).get(), 0.0, 0.005);
            },
            "Slope ~= 0"
            )
        .process();

    processAudioWith (Bypass())
        .withLabel ("White Noise generated in time domain")
        .withInputSignal (WhiteNoise())
        .withDuration (renderDurationSeconds)
        .expectTrue (
            [] (AnalysisContext context)
            {
                return HART_FLOAT_EQ (spectralLogLogSlope (context.outputSpectrum()).at (Slice::freq (20_Hz, 20_kHz)).get(), 0.0, 0.05);
            },
            "Slope ~= 0, with loose tolerance"
            )
        .process();

    processAudioWith (Bypass())
        .withLabel ("Linear Sine Sweep")
        .withInputSignal (SineSweep (renderDurationSeconds).withType (SineSweep::SweepType::linear))
        .withDuration (renderDurationSeconds)
        .expectTrue (
            [] (AnalysisContext context)
            {
                return HART_FLOAT_EQ (spectralLogLogSlope (context.outputSpectrum()).at (Slice::freq (20_Hz, 20_kHz)).get(), 0.0, 0.1);
            },
            "Slope ~= 0"
            )
        .process();

    processAudioWith (Bypass())
        .withLabel ("Log Sine Sweep")
        .withInputSignal (SineSweep (renderDurationSeconds).withType (SineSweep::SweepType::log))
        .withDuration (renderDurationSeconds)
        .expectTrue (
            [] (AnalysisContext context)
            {
                return HART_FLOAT_EQ (spectralLogLogSlope (context.outputSpectrum()).at (Slice::freq (20_Hz, 20_kHz)).get(), -1.0, 0.1);
            },
            "Slope ~= 0"
            )
        .process();
}

HART_TEST ("Metrics - Spectral Log-Log Slope - Units")
{
    using AudioBuffer = hart::AudioBuffer<float>;
    using hart::Spectrum;
    using hart::spectralLogLogSlope;
    using hart::powerToDecibels;

    AudioBuffer audioBuffer;
    processAudioWith (Bypass())
        .withLabel ("Pink Noise")
        .withInputSignal (Spectrum::colouredNoise (Spectrum::ColouredNoise::pink()).toSignal<float>())
        .inMono()
        .saveOutputTo (audioBuffer)
        .process();

    const Spectrum spectrum (audioBuffer);
    const double slopeDefault = spectralLogLogSlope (spectrum);
    const double slopeNative = spectralLogLogSlope (spectrum).as (native);
    const double slopeUnitless = spectralLogLogSlope (spectrum).as (none);
    const double slopeDbPerDecade = spectralLogLogSlope (spectrum).as (dB_per_decade);
    const double slopeDbPerOctave = spectralLogLogSlope (spectrum).as (dB_per_octave);
    const double threeDb = powerToDecibels (2.0);

    HART_EXPECT_FLOAT_EQ (slopeDefault, slopeNative, 1e-8) << "Default is same as native unit";
    HART_EXPECT_FLOAT_EQ (slopeNative, slopeUnitless, 1e-8) << "Native unit is unitless";
    HART_EXPECT_FLOAT_NE (slopeUnitless, slopeDbPerOctave, 1e-8) << "Unitless and dB per octave have different numeric values";
    HART_EXPECT_FLOAT_NE (slopeDbPerOctave, slopeDbPerDecade, 1e-8) << "dB per octave is different from dB per decade";
    HART_EXPECT_FLOAT_EQ (slopeDbPerDecade, slopeUnitless * 10.0, 1e-8) << "dB per decade is converted from unitless slope correctly";
    HART_EXPECT_FLOAT_EQ (slopeDbPerOctave, slopeUnitless * threeDb, 1e-8) << "dB per octave is converted from unitless slope correctly";
}

HART_TEST ("Metrics - Log Spectral Distance")
{
    using AudioBuffer = hart::AudioBuffer<float>;
    using hart::Normalise;
    using hart::Slice;
    using hart::Spectrum;
    using hart::logSpectralDistance;

    // 1. A few ideal spectra, constructed in frequency domain

    const Spectrum spectrumA = Spectrum::colouredNoise (Spectrum::ColouredNoise::pink().withRandomSeed (123));
    const Spectrum spectrumB = Spectrum::colouredNoise (Spectrum::ColouredNoise::pink().withRandomSeed (456));
    const Spectrum spectrumC = Spectrum::colouredNoise (Spectrum::ColouredNoise::white());

    HART_EXPECT_FLOAT_EQ (logSpectralDistance (spectrumA, spectrumB).get(), 0_dB, 1e-8)
        << "Different audio, but identical FFT bin magnitudes";

    HART_EXPECT_FLOAT_NE (logSpectralDistance (spectrumA, spectrumC).get(), 0_dB, 1e-8)
        << "Different spectra";

    const Spectrum spectrumD = spectrumA * 0.5;

    HART_EXPECT_FLOAT_EQ (logSpectralDistance (spectrumA, spectrumD).at (Slice::freq (20_Hz, 20_kHz)).get(), 6.02_dB, 0.01_dB)
        << "2x scaled spectrum - gain matters";

    HART_EXPECT_FLOAT_EQ (logSpectralDistance (spectrumA, spectrumD, Normalise::yes).at (Slice::freq (20_Hz, 20_kHz)).get(), 0_dB, 0.01_dB)
        << "2x scaled spectrum - gain doesn't matter if levels are normalised";

    // 2. White noise constructed in time domain vs white noise constructed in freq domain

    // Relatively long signal, with power-of-two duration in frames
    const hart::CLIConfig& cliConfig = hart::CLIConfig::getInstance();
    const double minRenderDurationSeconds = 1_s;
    const double sampleRateHz = cliConfig.getDefaultSampleRateHz();
    const size_t minRenderDurationFrames = hart::roundToSizeT (minRenderDurationSeconds * sampleRateHz);
    const size_t renderDurationFrames = hart::nextPowerOfTwo (minRenderDurationFrames);
    const double renderDurationSeconds = static_cast<double> (renderDurationFrames) / sampleRateHz;

    AudioBuffer audioBuffer;
    processAudioWith (Bypass())
        .withInputSignal (WhiteNoise())
        .withDuration (renderDurationSeconds)
        .saveOutputTo (audioBuffer)
        .process();
    const Spectrum spectrumE (audioBuffer);
    const Spectrum spectrumF = Spectrum::colouredNoise (Spectrum::ColouredNoise::white().withDuration (renderDurationFrames));

    HART_EXPECT_FLOAT_EQ (logSpectralDistance (spectrumE, spectrumF, Normalise::yes).at (Slice::freq (20_Hz, 20_kHz)).get(), 0_dB, 3_dB)
        << "Two different white noise generators should yield similar spectra, within 3 dB tolerance";

    // 3. Linear sine sweep sprctrum vs ideal white noise spectrum

    processAudioWith (Bypass())
        .withInputSignal (SineSweep().withType (SineSweep::SweepType::linear))
        .withDuration (renderDurationSeconds)
        .saveOutputTo (audioBuffer)
        .process();
    const Spectrum spectrumG (audioBuffer);

    HART_EXPECT_FLOAT_EQ (logSpectralDistance (spectrumF, spectrumG, Normalise::yes).at (Slice::freq (20_Hz, 20_kHz)).get(), 0_dB, 3_dB)
        << "Linear sine sweep should have similar spectrum to white noise, witin 3 dB tolerance";
}
