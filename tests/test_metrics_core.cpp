#include <array>
#include <utility>  // pair
#include <vector>

#include "hart.hpp"
#include "mock_metrics.hpp"

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

HART_PARAMETRISED_TEST_WITH_TAGS ("Windowed MetricQuery - Single buffer - Setting window size results in correct amount of windows if buffer size is a multiple of window size", "[Metrics][WindowedMetricQuery]")
{
    using hart::first;
    using hart::size;

    const double windowSizeSeconds = HART_GENERATE_VALUE (1_ms, 4_ms, 11_ms, 44_ms, 88_ms);
    HART_CAPTURE_VALUE (windowSizeSeconds);

    const AudioBuffer buffer = processAudioWith (Bypass())
        .withInputSignal (Silence())
        .withDuration (88_ms)
        .inMono()
        .processAndGetOutputAudio();

    const size_t numWindowsObserved = windowed (buffer)
        .metric ([] (const AudioBuffer& window) { return justReturnZeros (window).collect(); })
        .size (windowSizeSeconds)
        .get (first(), size());

    HART_EXPECT_FLOAT_EQ (static_cast<double> (numWindowsObserved), 88_ms / windowSizeSeconds, 1e-8);
}

HART_PARAMETRISED_TEST_WITH_TAGS ("Windowed MetricQuery - Buffer pair - Setting window size results in correct amount of windows if buffer size is a multiple of window size", "[Metrics][WindowedMetricQuery]")
{
    using hart::first;
    using hart::size;

    const double windowSizeSeconds = HART_GENERATE_VALUE (1_ms, 5_ms, 12_ms, 20_ms, 60_ms);
    HART_CAPTURE_VALUE (windowSizeSeconds);

    const AudioBuffer bufferA = processAudioWith (Bypass())
        .withInputSignal (Silence())
        .withDuration (60_ms)
        .inMono()
        .processAndGetOutputAudio();
    const AudioBuffer bufferB (bufferA);

    const size_t numWindowsObserved = windowed (bufferA, bufferB)
        .metric ([] (const AudioBuffer& windowA, const AudioBuffer& /* windowB */) { return justReturnZeros (windowA).collect(); })
        .size (windowSizeSeconds)
        .get (first(), size());

    HART_EXPECT_FLOAT_EQ (static_cast<double> (numWindowsObserved), 60_ms / windowSizeSeconds, 1e-8);
}

HART_PARAMETRISED_TEST_WITH_TAGS ("Windowed MetricQuery - Single buffer - Setting window size results in correct window sizes if buffer size is a multiple of window size", "[Metrics][WindowedMetricQuery]")
{
    using hart::allFloatsEqualTo;
    using hart::first;

    const double windowSizeSeconds = HART_GENERATE_VALUE (1_ms, 5_ms, 10_ms, 25_ms, 50_ms);
    HART_CAPTURE_VALUE (windowSizeSeconds);

    const AudioBuffer buffer = processAudioWith (Bypass())
        .withInputSignal (Silence())
        .withDuration (50_ms)
        .inMono()
        .processAndGetOutputAudio();

    // Allow one sample, without specifying a rounding rule for durations.
    const double toleranceSeconds = 1.0 / buffer.getSampleRateHz();
    const bool allWindowSizesAreCorrect = windowed (buffer)
        .metric ([] (const AudioBuffer& window) { return justReturnBufferSizes (window).as (seconds).collect(); })
        .size (windowSizeSeconds)
        .get (first(), allFloatsEqualTo (windowSizeSeconds, toleranceSeconds));

    HART_EXPECT_TRUE (allWindowSizesAreCorrect);
}

HART_PARAMETRISED_TEST_WITH_TAGS ("Windowed MetricQuery - Buffer pair - Setting window size results in correct window sizes if buffer size is a multiple of window size", "[Metrics][WindowedMetricQuery]")
{
    using hart::allFloatsEqualTo;
    using hart::first;

    const double windowSizeSeconds = HART_GENERATE_VALUE (1_ms, 9_ms, 10_ms, 30_ms, 90_ms);
    HART_CAPTURE_VALUE (windowSizeSeconds);

    const AudioBuffer bufferA = processAudioWith (Bypass())
        .withInputSignal (Silence())
        .withDuration (90_ms)
        .inMono()
        .processAndGetOutputAudio();
    const AudioBuffer bufferB (bufferA);

    const double toleranceSeconds = 1_frames / bufferA.getSampleRateHz();
    const auto query = windowed (bufferA, bufferB).size (windowSizeSeconds);

    const bool allWindowASizesAreCorrect = query
        .metric ([] (const AudioBuffer& windowA, const AudioBuffer& /* windowB */) { return justReturnBufferSizes (windowA).as (seconds).collect(); })
        .get (first(), allFloatsEqualTo (windowSizeSeconds, toleranceSeconds));

    const bool allWindowBSizesAreCorrect = query
        .metric ([] (const AudioBuffer& /* windowA */, const AudioBuffer& windowB) { return justReturnBufferSizes (windowB).as (seconds).collect(); })
        .get (first(), allFloatsEqualTo (windowSizeSeconds, toleranceSeconds));

    HART_EXPECT_TRUE (allWindowASizesAreCorrect);
    HART_EXPECT_TRUE (allWindowBSizesAreCorrect);
}

HART_TEST_WITH_TAGS ("Windowed MetricQuery - Configuration survives query copy", "[Metrics][WindowedMetricQuery]")
{
    using hart::first;
    using hart::size;

    const AudioBuffer buffer = processAudioWith (Bypass())
        .withInputSignal (Silence())
        .withDuration (100_ms)
        .inMono()
        .processAndGetOutputAudio();

    const auto singleQuery = windowed (buffer)
        .size (20_ms).step (10_ms)
        .metric ([] (const AudioBuffer& window) { return justReturnZeros (window).collect(); });

    const auto singleCopy = singleQuery;
    const auto resizedSingleCopy = singleCopy.size (10_ms);

    HART_EXPECT_EQ (singleCopy.get (first(), size()), 9ul);
    HART_EXPECT_EQ (resizedSingleCopy.get (first(), size()), 10ul);
    HART_EXPECT_EQ (singleQuery.get (first(), size()), 9ul);

    const auto pairQuery = windowed (buffer, buffer)
        .size (20_ms).step (10_ms)
        .metric ([] (const AudioBuffer& windowA, const AudioBuffer&) { return justReturnZeros (windowA).collect(); });

    const auto pairCopy = pairQuery;
    const auto resizedPairCopy = pairCopy.size (10_ms);

    HART_EXPECT_EQ (pairCopy.get (first(), size()), 9ul);
    HART_EXPECT_EQ (resizedPairCopy.get (first(), size()), 10ul);
    HART_EXPECT_EQ (pairQuery.get (first(), size()), 9ul);
}

HART_TEST_WITH_TAGS ("Windowed MetricQuery - Configuration survives value type change", "[Metrics][WindowedMetricQuery]")
{
    const AudioBuffer buffer = processAudioWith (Bypass())
        .withInputSignal (Silence())
        .withDuration (100_ms)
        .inMono()
        .processAndGetOutputAudio();
    const std::vector<int> values { 0 };

    const auto singleQuery = windowed (buffer)
        .size (20_ms).step (10_ms)
        .metric ([] (const AudioBuffer& window) { return justReturnZeros (window).collect(); });

    const auto singleIntQuery = singleQuery
        .metric ([&values] (const AudioBuffer&) { return mockMetric (values).collect(); });

    HART_EXPECT_EQ (singleIntQuery.get (hart::first(), hart::size()), size_t (9));

    const auto pairQuery = windowed (buffer, buffer).size (20_ms).step (10_ms)
        .metric ([] (const AudioBuffer& windowA, const AudioBuffer&) { return justReturnZeros (windowA).collect(); });

    const auto pairIntQuery = pairQuery
        .metric ([&values] (const AudioBuffer&, const AudioBuffer&) { return mockMetric (values).collect(); });

    HART_EXPECT_EQ (pairIntQuery.get (hart::first(), hart::size()), size_t (9));
}

HART_PARAMETRISED_TEST_WITH_TAGS ("Windowed MetricQuery - Single buffer - Each frame is used exactly once", "[Metrics][WindowedMetricQuery]")
{
    using hart::first;
    using hart::size;

    const double windowSizeSeconds = HART_GENERATE_VALUE (1_ms, 5_ms, 25_ms);
    HART_CAPTURE_VALUE (windowSizeSeconds);

    const auto ramp = SegmentedEnvelope (0.0).rampTo (1.0, 1_s);
    const AudioBuffer audio = processAudioWith (Bypass())
        .withInputSignal (DC (1.0f) >> GainLinear().withEnvelope (GainLinear::gainLinear, ramp))
        .withDuration (1_s)
        .inMono()
        .processAndGetOutputAudio();

    size_t framesSeen = 0;
    bool allFramesAreInOrder = true;

    windowed (audio).size (windowSizeSeconds)
        .metric (
            [&] (const AudioBuffer& window)
            {
                // A DC ramp gives each source frame a distinct value. Check the
                // concatenated windows against it, rather than just adding samples.
                for (size_t frame = 0; frame < window.getNumFrames(); ++frame)
                {
                    if (framesSeen >= audio.getNumFrames() || window[0][frame] != audio[0][framesSeen])
                        allFramesAreInOrder = false;

                    ++framesSeen;
                }
                return justReturnZeros (window).collect();
            }
        )
        .get (first(), size());

    HART_EXPECT_TRUE (allFramesAreInOrder);
    HART_EXPECT_EQ (framesSeen, audio.getNumFrames());
}

HART_PARAMETRISED_TEST_WITH_TAGS ("Windowed MetricQuery - Buffer pair - Each frame is used exactly once", "[Metrics][WindowedMetricQuery]")
{
    using hart::first;
    using hart::size;

    const double windowSizeSeconds = HART_GENERATE_VALUE (1_ms, 5_ms, 25_ms);
    HART_CAPTURE_VALUE (windowSizeSeconds);

    const auto ramp = SegmentedEnvelope (0.0).rampTo (1.0, 1_s);
    const AudioBuffer audioA = processAudioWith (Bypass())
        .withInputSignal (DC (1.0f) >> GainLinear().withEnvelope (GainLinear::gainLinear, ramp))
        .withDuration (1_s)
        .inMono()
        .processAndGetOutputAudio();
    AudioBuffer audioB (audioA);
    audioB.processWith (GainLinear (-1.0));

    size_t framesSeen = 0;
    bool allFramesAreInOrder = true;

    windowed (audioA, audioB).size (windowSizeSeconds)
        .metric (
            [&] (const AudioBuffer& windowA, const AudioBuffer& windowB)
            {
                HART_EXPECT_EQ (windowA.getNumFrames(), windowB.getNumFrames());

                for (size_t frame = 0; frame < windowA.getNumFrames(); ++frame)
                {
                    if (
                        framesSeen >= audioA.getNumFrames()
                        || windowA[0][frame] != audioA[0][framesSeen]
                        || frame >= windowB.getNumFrames()
                        || windowB[0][frame] != audioB[0][framesSeen]
                    )
                    {
                        allFramesAreInOrder = false;
                    }

                    ++framesSeen;
                }

            return justReturnZeros (windowA).collect();
            }
        )
        .get (first(), size());

    HART_EXPECT_TRUE (allFramesAreInOrder);
    HART_EXPECT_EQ (framesSeen, audioA.getNumFrames());
}

HART_PARAMETRISED_TEST_WITH_TAGS ("Windowed MetricQuery - Setting a custom window step results in correct number of windows", "[Metrics][WindowedMetricQuery]")
{
    using hart::windowed;
    using hart::first;
    using hart::size;

    constexpr double renderDurationSeconds = 100_ms;
    constexpr double windowSizeSeconds = 10_ms;

    constexpr std::array<std::pair<double, size_t>, 4> windowStepsAndExpectedNumbersOfWindows = {{
        { 10_ms, 10ul },
        { 5_ms, 19ul },
        { 1_ms, 91ul },
        { 50_ms, 2ul }
    }};

    const auto windowStepAndExpectedNumberOfWindows = HART_GENERATE_VALUE (windowStepsAndExpectedNumbersOfWindows);
    const double windowStepSeconds = windowStepAndExpectedNumberOfWindows.first;
    const size_t expectedNumWindows = windowStepAndExpectedNumberOfWindows.second;
    HART_CAPTURE_VALUE (windowStepSeconds);
    HART_CAPTURE_VALUE (expectedNumWindows);

    const AudioBuffer audio = processAudioWith (Bypass())
        .withInputSignal (Silence())
        .withDuration (renderDurationSeconds)
        .inMono()
        .processAndGetOutputAudio();

    const size_t observedNumWindows = windowed (audio)
        .metric ([] (const AudioBuffer& window) { return justReturnZeros (window).collect(); })
        .size (windowSizeSeconds)
        .step (windowStepSeconds)
        .get (first(), size());

    HART_EXPECT_EQ (observedNumWindows, expectedNumWindows);
}

HART_PARAMETRISED_TEST_WITH_TAGS ("Windowed MetricQuery - Setting a custom window step results in correct window sizes", "[Metrics][WindowedMetricQuery]")
{
    using hart::windowed;
    using hart::first;
    using hart::allFloatsEqualTo;
    using hart::CLIConfig;

    const double windowStepSeconds = HART_GENERATE_VALUE (1_ms, 7_ms, 10_ms, 42_ms, 99_ms, 100_ms, 999_s);
    HART_CAPTURE_VALUE (windowStepSeconds);

    const AudioBuffer audio = processAudioWith (Bypass())
        .withInputSignal (Silence())
        .withDuration (100_ms)
        .inMono()
        .processAndGetOutputAudio();

    const double toleranceSeconds = 1_frames / CLIConfig::getInstance().getDefaultSampleRateHz();

    const bool allWindowSizesAreCorrect = windowed (audio)
        .metric ([] (const AudioBuffer& window) { return justReturnBufferSizes (window).as (seconds).collect(); })
        .size (10_ms)
        .step (windowStepSeconds)
        .get (first(), allFloatsEqualTo (10_ms, toleranceSeconds));

    HART_EXPECT_TRUE (allWindowSizesAreCorrect);
}
