#include "hart.hpp"
#include "mock_metric.hpp"

HART_DECLARE_ALIASES_FOR_FLOAT;
HART_DECLARE_ALIASES_FOR_UNITS;
using AudioBuffer = hart::AudioBuffer<float>;

// TODO: Move the existing metric query tests cases here

HART_TEST_WITH_TAGS ("MetricQuery - collect() just collects all the per-channel values", "[Metrics][MetricQuery]")
{
    const std::vector<int> values { 11, 22, 33 };
    const std::vector<int> extractedValues = mockMetric (values).collect();

    HART_EXPECT_EQ (extractedValues, values);
}

HART_TEST_WITH_TAGS ("Windowed MetricQuery - Single buffer - get() picks correct values with default axis", "[Metrics][WindowedMetricQuery]")
{
    using hart::windowed;
    using hart::max;
    using hart::min;
    using hart::roundToSizeT;

    const std::vector<std::vector<int>> values = {
        { 11, 22, 33 },
        { 444, 555, 666 },
        { 7777, 8888, 9999 }
    };

    const double sampleRateHz = 44100_Hz;
    const size_t dummyBufferSizeFrames = roundToSizeT (sampleRateHz * 300_ms);
    const AudioBuffer dummyBuffer (3, dummyBufferSizeFrames, sampleRateHz);
    auto mockMetric = mockWindowedMetric (values);

    HART_EXPECT_EQ (
        windowed (dummyBuffer)
            .metric ([mockMetric] (const AudioBuffer&) mutable { return mockMetric().collect(); })
            .get (min(), min()),
        11
        );

    HART_EXPECT_EQ (
        windowed (dummyBuffer)
            .metric ([mockMetric] (const AudioBuffer&) mutable { return mockMetric().collect(); })
            .get (max(), max()),
        9999
        );

    HART_EXPECT_EQ (
        windowed (dummyBuffer)
            .metric ([mockMetric] (const AudioBuffer&) mutable { return mockMetric().collect(); })
            .get (max(), min()),
        7777
        );

    HART_EXPECT_EQ (
        windowed (dummyBuffer)
            .metric ([mockMetric] (const AudioBuffer&) mutable { return mockMetric().collect(); })
            .get (min(), max()),
        33
        );
}

HART_TEST_WITH_TAGS ("Windowed MetricQuery - Buffer pair - get() picks correct values with default axis", "[Metrics][WindowedMetricQuery]")
{
    using hart::windowed;
    using hart::max;
    using hart::min;
    using hart::roundToSizeT;

    const std::vector<std::vector<int>> values = {
        { 77, 88, 99 },
        { 111, 555, 333 },
        { 9988, 8877, 6655 }
    };

    const double sampleRateHz = 44100_Hz;
    const size_t dummyBufferSizeFrames = roundToSizeT (sampleRateHz * 300_ms);
    const AudioBuffer dummyBufferA (3, dummyBufferSizeFrames, sampleRateHz);
    const AudioBuffer dummyBufferB (dummyBufferA);
    auto mockMetric = mockWindowedMetric (values);

    HART_EXPECT_EQ (
        windowed (dummyBufferA, dummyBufferB)
            .metric ([mockMetric] (const AudioBuffer&, const AudioBuffer&) mutable { return mockMetric().collect(); })
            .get (min(), min()),
        77
        );

    HART_EXPECT_EQ (
        windowed (dummyBufferA, dummyBufferB)
            .metric ([mockMetric] (const AudioBuffer&, const AudioBuffer&) mutable { return mockMetric().collect(); })
            .get (max(), max()),
        9988
        );

    HART_EXPECT_EQ (
        windowed (dummyBufferA, dummyBufferB)
            .metric ([mockMetric] (const AudioBuffer&, const AudioBuffer&) mutable { return mockMetric().collect(); })
            .get (max(), min()),
        6655
        );

    HART_EXPECT_EQ (
        windowed (dummyBufferA, dummyBufferB)
            .metric ([mockMetric] (const AudioBuffer&, const AudioBuffer&) mutable { return mockMetric().collect(); })
            .get (min(), max()),
        99
        );
}

HART_TEST_WITH_TAGS ("Windowed MetricQuery - Single buffer - get() accepts non-default axes", "[Metrics][WindowedMetricQuery]")
{
    using hart::Axis;
    using hart::windowed;
    using hart::max;
    using hart::min;
    using hart::roundToSizeT;

    const std::vector<std::vector<int>> values = {
        { 11, 22, 33 },
        { 444, 555, 666 },
        { 7777, 8888, 9999 }
    };

    const double sampleRateHz = 44100_Hz;
    const size_t dummyBufferSizeFrames = roundToSizeT (sampleRateHz * 300_ms);
    const AudioBuffer dummyBuffer (3, dummyBufferSizeFrames, sampleRateHz);
    auto mockMetric = mockWindowedMetric (values);

    HART_EXPECT_EQ (
        windowed (dummyBuffer)
            .metric ([mockMetric] (const AudioBuffer&) mutable { return mockMetric().collect(); })
            .get (min(), max(), Axis::channels),
        33
        ) << "Second reducer is applied to each channel";

    HART_EXPECT_EQ (
        windowed (dummyBuffer)
            .metric ([mockMetric] (const AudioBuffer&) mutable { return mockMetric().collect(); })
            .get (min(), max(), Axis::frames),
        7777
        ) << "Second reducer is applied to each window";

    HART_EXPECT_EQ (
        windowed (dummyBuffer)
            .metric ([mockMetric] (const AudioBuffer&) mutable { return mockMetric().collect(); })
            .get (min(), max()),
        33
        ) << "Default axis is Axis::channels";
}

HART_TEST_WITH_TAGS ("Windowed MetricQuery - Buffer pair - get() accepts non-default axes", "[Metrics][WindowedMetricQuery]")
{
    using hart::Axis;
    using hart::windowed;
    using hart::max;
    using hart::min;
    using hart::roundToSizeT;

    const std::vector<std::vector<int>> values = {
        { 85, 25, 35 },
        { 200, 800, 400 },
        { 9000, 7000, 5000 }
    };

    const double sampleRateHz = 44100_Hz;
    const size_t dummyBufferSizeFrames = roundToSizeT (sampleRateHz * 300_ms);
    const AudioBuffer dummyBufferA (3, dummyBufferSizeFrames, sampleRateHz);
    const AudioBuffer dummyBufferB (3, dummyBufferSizeFrames, sampleRateHz);
    auto mockMetric = mockWindowedMetric (values);

    HART_EXPECT_EQ (
        windowed (dummyBufferA, dummyBufferB)
            .metric ([mockMetric] (const AudioBuffer&, const AudioBuffer&) mutable { return mockMetric().collect(); })
            .get (min(), max(), Axis::channels),
        85
        ) << "Second reducer is applied to each channel";

    HART_EXPECT_EQ (
        windowed (dummyBufferA, dummyBufferB)
            .metric ([mockMetric] (const AudioBuffer&, const AudioBuffer&) mutable { return mockMetric().collect(); })
            .get (min(), max(), Axis::frames),
        5000
        ) << "Second reducer is applied to each window";

    HART_EXPECT_EQ (
        windowed (dummyBufferA, dummyBufferB)
            .metric ([mockMetric] (const AudioBuffer&, const AudioBuffer&) mutable { return mockMetric().collect(); })
            .get (min(), max()),
        85
        ) << "Default axis is Axis::channels";
}

HART_TEST_WITH_TAGS ("Windowed MetricQuery - Single buffer - Works with mono signal correctly", "[Metrics][WindowedMetricQuery]")
{
    using hart::first;
    using hart::min;
    using hart::max;
    using hart::mean;
    using hart::zcr;

    const AudioBuffer audio = processAudioWith (Bypass())
        .withInputSignal (SineSweep (1_s, 7_Hz, 7_kHz, SineSweep::SweepType::linear))
        .withDuration (1_s)
        .inMono()
        .processAndGetOutputAudio();

    const auto query = hart::windowed (audio)
        .metric ([] (const AudioBuffer& frame) { return zcr (frame).collect(); });

    const double highestZcrHz = query.get (first(), max());
    const double lowestZcrHz = query.get (first(), min());
    const double meanZcrHz = query.get (first(), mean());

    // Reminder just in case: ZCR is typically ~2x of actual frequency

    HART_EXPECT_GT (highestZcrHz, 10.0 * lowestZcrHz) << "Highest ZCR is significantly higher that the lowest one";
    HART_EXPECT_FLOAT_IN_RANGE (highestZcrHz, 7_kHz, 14_kHz, 1_Hz) << "7 kHz <= Highest ZCR <= 14 kHz";
    HART_EXPECT_FLOAT_IN_RANGE (lowestZcrHz, 14_Hz, 7_kHz, 1_Hz) << "7 Hz <= Lowest ZCR <= 7 kHz";
    HART_EXPECT_FREQ_EQ (meanZcrHz, 0.5 * (highestZcrHz + lowestZcrHz), 10_cents) << "Mean ZCR is right in the middle between max and min in linear sweep";
}

HART_TEST_WITH_TAGS ("Windowed MetricQuery - Single buffer - Works with multi-channel signals correctly", "[Metrics][WindowedMetricQuery]")
{
    using hart::Channel;
    using hart::windowed;
    using hart::min;
    using hart::max;
    using hart::nth;

    const auto risingEnvelope = SegmentedEnvelope (0.0).rampTo (1.0, 1_s, SegmentedEnvelope::Shape::linear);

    auto testSignal = NyquistSignal()
        >> GainLinear().withEnvelope (GainLinear::gainLinear, risingEnvelope)
        >> GainDb (-6_dB).atChannel (Channel::left)
        >> GainDb (-7_dB).atChannel (Channel::right);

    const AudioBuffer audio = processAudioWith (Bypass())
        .withInputSignal (std::move (testSignal))
        .withDuration (1_s)
        .inStereo()
        .processAndGetOutputAudio();

    const auto query = windowed (audio).metric ([] (const AudioBuffer& frame) { return hart::samplePeak (frame).as (dB).collect(); });

    HART_EXPECT_FLOAT_EQ (
        query.get (nth (Channel::left), max()),
        -6_dB,
        0.1_dB
    ) << "Loudest window in left channel peaks at -6 dB";

    HART_EXPECT_LT (
        query.get (nth (Channel::left), min()),
        -12_dB
    ) << "Quietest window in left channel peaks below -12 dB";

    HART_EXPECT_FLOAT_EQ (
        query.get (nth (Channel::right), max()),
        -7_dB,
        0.1_dB
    ) << "Loudest window in right channel peaks at -7 dB";

    HART_EXPECT_FLOAT_EQ (
        query.get (nth (Channel::right), min()),
        -1_dB + query.get (nth (Channel::left), min()),
        0.1_dB
    ) << "Quietest window in right channel peaks 1 dB lower than its left channel counterpart";
}

HART_TEST_WITH_TAGS ("Windowed MetricQuery - Buffer pair - Works with mono signal correctly", "[Metrics][WindowedMetricQuery]")
{
    using hart::first;
    using hart::min;
    using hart::max;
    using hart::mean;
    using hart::snr;

    const auto risingEnvelope = SegmentedEnvelope (0.0).rampTo (1.0, 1_s, SegmentedEnvelope::Shape::linear);

    const AudioBuffer noise = processAudioWith (Bypass())
        .withInputSignal (BinaryNoise() >> GainLinear().withEnvelope (GainLinear::gainLinear, risingEnvelope))
        .withDuration (1_s)
        .inMono()
        .processAndGetOutputAudio();

    const AudioBuffer signal = processAudioWith (Bypass())
        .withInputSignal (SineWave())
        .withDuration (1_s)
        .inMono()
        .processAndGetOutputAudio();

    const AudioBuffer signalPlusNoise = signal + noise;

    const auto query = hart::windowed (signalPlusNoise, signal)
        .metric ([] (const AudioBuffer& noisy, const AudioBuffer& clean) { return snr (noisy, clean).as (dB).collect(); });

    const double bestSnrDb = query.get (first(), max());
    const double worstSnrDb = query.get (first(), min());
    const double meanSnrDb = query.get (first(), mean());

    HART_EXPECT_GT (bestSnrDb, 10_dB + worstSnrDb) << "Best SNR is significantly higher that the worst one";
    HART_EXPECT_FLOAT_IN_RANGE (bestSnrDb, 0_dB, 100_dB, 0.1_dB) << "0 dB <= Best SNR <= 100 dB";
    HART_EXPECT_FLOAT_IN_RANGE (worstSnrDb, -6_dB, 0_dB, 0.1_dB) << "-6 dB <= Worst SNR <= 0 dB";
    // SNR in dB is nonlinear in the noise amplitude, so its mean need not be halfway between the extrema.
    HART_EXPECT_GT (meanSnrDb, worstSnrDb);
    HART_EXPECT_LT (meanSnrDb, bestSnrDb);
}

HART_TEST_WITH_TAGS ("Windowed MetricQuery - Buffer pair - Works with multi-channel signals correctly", "[Metrics][WindowedMetricQuery]")
{
    using hart::Channel;
    using hart::windowed;
    using hart::maxCrossCorrelation;
    using hart::nth;
    using hart::first;
    using hart::last;
    using std::abs;

    const auto risingEnvelope = SegmentedEnvelope (0.0).rampTo (1.0, 1_s);
    const auto fallingEnvelope = SegmentedEnvelope (1.0).rampTo (0.0, 1_s);

    auto noisySignal =
        (SineWave() >> GainLinear().withEnvelope (GainLinear::gainLinear, fallingEnvelope))
        + (BinaryNoise()
            >> GainLinear().atChannel (Channel::left).withEnvelope (GainLinear::gainLinear, risingEnvelope)
            >> Mute().atChannel (Channel::right));

    const AudioBuffer noisyAudio = processAudioWith (Bypass())
        .withInputSignal (std::move (noisySignal))
        .withDuration (1_s)
        .inStereo()
        .processAndGetOutputAudio();

    const AudioBuffer cleanAudio = processAudioWith (Bypass())
        .withInputSignal (SineWave())
        .withDuration (1_s)
        .inStereo()
        .processAndGetOutputAudio();

    const auto query = windowed (cleanAudio, noisyAudio)
        .metric ([] (const AudioBuffer& windowA, const AudioBuffer& windowB) { return maxCrossCorrelation (windowA, windowB, 0_ms).apply (abs).collect(); });

    HART_EXPECT_GT (
        query.get (nth (Channel::left), first()),
        0.9
    ) << "Signals are well correlated in first window on left channel";

    HART_EXPECT_GT (
        query.get (nth (Channel::right), last()),
        0.5
    ) << "Signals are moderately correlated in last window on right channel";

    HART_EXPECT_LT (
        query.get (nth (Channel::left), last()),
        0.2
    ) << "Signals are poorly correlated in last window on left channel";
}
