#include "hart.hpp"

HART_DECLARE_ALIASES_FOR_FLOAT;
HART_DECLARE_ALIASES_FOR_UNITS;
using hart::Loop;
using hart::decibelsToRatio;

HART_TEST ("Signal - Fast Forward via skipTo()")
{
    // 1. Actual polarity flip, or GainLinear(-1.0) lands on a perfect phase.
    // 2. Setting starting phase in SineWave ctor lands on a somewhat accurate phase, but not perfect due to FP error and pi being pi.
    // 3. Changing phase via nudging in time domain - pretty meh phase, since we only have 1 frame resolution and FP error.
    // ...So the tolerance is pretty loose here.
    constexpr double toleranceLinear = 1.0e-2;

    processAudioWith (GainDb())
        .withLabel ("Skip zero seconds")
        .withInputSignal (SineWave().skipTo (0_s))
        .expectTrue (EqualsTo (SineWave(), toleranceLinear))
        .process();

    processAudioWith (GainDb())
        .withLabel ("Skip arbitrary small amount")
        .withInputSignal (SineWave().skipTo (12.3_ms))
        .expectFalse (EqualsTo (SineWave(), toleranceLinear))
        .process();

    processAudioWith (GainDb())
        .withLabel ("Skip arbitrary significant amount")
        .withInputSignal (SineWave().skipTo (1.23456_s))
        .expectFalse (EqualsTo (SineWave(), toleranceLinear))
        .process();

    processAudioWith (GainDb())
        .withSampleRate (44100_Hz)
        .withLabel ("Skip exactly one cycle at 44.1kHz")
        .withInputSignal (SineWave (60_Hz).skipTo (1.0 / 60_Hz))
        .expectTrue (EqualsTo (SineWave (60_Hz), toleranceLinear))
        .process();

    processAudioWith (GainDb())
        .withSampleRate (48000_Hz)
        .withLabel ("Skip exactly one cycle at 48kHz")
        .withInputSignal (SineWave (440_Hz).skipTo (1.0 / 440_Hz))
        .expectTrue (EqualsTo (SineWave (440_Hz), toleranceLinear))
        .process();

    processAudioWith (GainDb())
        .withLabel ("Skip exactly four cycles")
        .withInputSignal (SineWave (100_Hz).skipTo (4.0 / 100_Hz))
        .expectTrue (EqualsTo (SineWave (100_Hz), toleranceLinear))
        .process();

    processAudioWith (GainDb())
        .withLabel ("Skip a hundred full cycles")
        .withInputSignal (SineWave (123_Hz).skipTo (100.0 / 123_Hz))
        .expectTrue (EqualsTo (SineWave (123_Hz), toleranceLinear))
        .process();

    processAudioWith (GainDb())
        .withLabel ("Skip exactly half a cycle")
        .withInputSignal (SineWave (60_Hz).skipTo (0.5 / 60_Hz))
        .expectFalse (EqualsTo (SineWave (60_Hz), toleranceLinear))
        .expectTrue (EqualsTo (SineWave (60_Hz) >> GainLinear (-1.0), toleranceLinear))
        .expectTrue (EqualsTo (SineWave (60_Hz, hart::pi), toleranceLinear))
        .process();

    const double defaultSampleRateHz = hart::CLIConfig::getInstance().getDefaultSampleRateHz();
    const double skipChunkSeconds = 0.5 / 440_Hz;
    const size_t skipChunkFrames = hart::roundToSizeT (defaultSampleRateHz * skipChunkSeconds);
    const double skipChunkRadians = hart::twoPi * 440_Hz * skipChunkFrames / defaultSampleRateHz;
    processAudioWith (GainDb())
        .withLabel ("Skip exactly half a cycle in reference signal")
        .withInputSignal (SineWave (440_Hz, skipChunkRadians))
        .expectTrue (EqualsTo (SineWave (440_Hz).skipTo (0.5 / 440_Hz), toleranceLinear))
        .process();

    processAudioWith (GainDb())
        .withLabel ("Skip to the end of signal")
        .withInputSignal (SineSweep (200_ms).withLoop (Loop::no).skipTo (201_ms))
        .expectTrue (PeaksAt (-oo_dB))
        .process();

    processAudioWith (GainDb())
        .withLabel ("Skiping multiple times")
        .withInputSignal (SineWave().skipTo (121_ms).skipTo (29.1_ms))
        .expectTrue (EqualsTo (SineWave().skipTo (90_ms).skipTo (35_ms).skipTo (25.1_ms), toleranceLinear))
        .expectFalse (EqualsTo (SineWave(), toleranceLinear))
        .process();
}

HART_TEST ("Signal - Unary Flip")
{
    processAudioWith (GainDb())
        .withLabel ("Unary minus")
        .withInputSignal (-SineWave (440_Hz))
        .expectFalse (EqualsTo (SineWave (440_Hz)))
        .expectTrue (EqualsTo (SineWave (440_Hz, hart::pi)))
        .process();

    processAudioWith (GainDb())
        .withLabel ("Unary tilda")
        .withInputSignal (~SineWave (5_kHz))
        .expectFalse (EqualsTo (SineWave (5_kHz)))
        .expectTrue (EqualsTo (SineWave (5_kHz, hart::pi)))
        .process();

    processAudioWith (GainDb())
        .withLabel ("Minus vs tilda")
        .withInputSignal (~SineWave (60_Hz))
        .expectFalse (EqualsTo (SineWave (60_Hz)))
        .expectTrue (EqualsTo (-SineWave (60_Hz)))
        .process();

    processAudioWith (GainDb())
        .withLabel ("Signal with DSP chain")
        .withInputSignal (-SineWave() >> HardClip (-3_dB))
        .expectFalse (EqualsTo (SineWave() >> HardClip (-3_dB)))
        .expectTrue (EqualsTo (SineWave() >> GainLinear (-1.0) >> HardClip (-3_dB)))
        .process();
}

HART_TEST ("Signal - Mixing Signals")
{
    processAudioWith (GainDb())
        .withLabel ("Adding two identical signals")
        .withInputSignal (SineWave() + SineWave())
        .expectFalse (EqualsTo (SineWave()))
        .expectTrue (EqualsTo (SineWave() >> GainLinear (2.0)))
        .process();

    processAudioWith (GainDb())
        .withLabel ("Subtracting two identical signals")
        .withInputSignal (SineWave() - SineWave())
        .expectTrue (PeaksAt (-oo_dB))
        .process();

    processAudioWith (GainDb())
        .withLabel ("Adding two out of phase signals")
        .withInputSignal (SineWave (440_Hz) + SineWave (440_Hz, hart::pi))
        .expectTrue (PeaksAt (-oo_dB))
        .process();

    processAudioWith (GainDb())
        .withLabel ("Subtracting two out of phase signals")
        .withInputSignal (SineWave (440_Hz) - SineWave (440_Hz, hart::pi))
        .expectTrue (EqualsTo (SineWave (440_Hz) >> GainLinear (2.0)))
        .process();

    processAudioWith (GainDb())
        .withLabel ("Order doesn't matter")
        .withInputSignal (SineWave (440_Hz) + SineWave (100_Hz))
        .expectFalse (EqualsTo (SineWave (440_Hz)))
        .expectFalse (EqualsTo (SineWave (100_Hz)))
        .expectTrue (EqualsTo (SineWave (100_Hz) + SineWave (440_Hz)))
        .process();

    processAudioWith (GainDb())
        .withLabel ("Adding a bunch of identical signals")
        .withInputSignal (SineWave() + SineWave() + SineWave() + SineWave() + SineWave())
        .expectFalse (EqualsTo (SineWave()))
        .expectTrue (EqualsTo (SineWave() >> GainLinear (5.0)))
        .process();

    processAudioWith (GainDb())
        .withLabel ("Preserving signal chains")
        .withInputSignal ((SineWave() >> GainLinear (1.23)) + (SineWave() >> GainLinear (4.56) >> GainLinear (3.21)))
        .expectFalse (EqualsTo (SineWave()))
        .expectTrue (EqualsTo (SineWave() >> GainLinear (1.23 + 4.56 * 3.21)))
        .process();
}

HART_TEST ("Signal - Sawtooth Frequency")
{
    const double defaultRenderDuration = hart::CLIConfig::getInstance().getDefaultRenderDurationSeconds();

    processAudioWith (GainDb())
        .withLabel ("Normal use")
        .withInputSignal (Sawtooth())
        .expectTrue (PeaksAt (0_dB))
        .process();

    processAudioWith (GainDb())
        .withLabel ("Arbitraty phase")
        .withInputSignal (Sawtooth (1000_Hz, 1.2345_rad))
        .expectTrue (PeaksAt (0_dB, 0.02))
        .expectFalse (EqualsTo (Sawtooth()))
        .process();

    processAudioWith (GainDb())
        .withLabel ("TwoPi starting phase")
        .withInputSignal (Sawtooth (1000_Hz, hart::twoPi))
        .expectTrue (PeaksAt (0_dB))
        .expectTrue (EqualsTo (Sawtooth()))
        .process();

    processAudioWith (GainDb())
        .withLabel ("Default Frequency")
        .withDuration (std::max (defaultRenderDuration, 100_ms))
        .withInputSignal (Sawtooth())
        .expectTrue (PeaksAt (0_dB))
        .expectTrue (FundamentalEquals (1000_Hz))
        .process();

    processAudioWith (GainDb())
        .withLabel ("Arbitrary Frequency")
        .withDuration (std::max (defaultRenderDuration, 100_ms))
        .withInputSignal (Sawtooth (440_Hz))
        .expectTrue (PeaksAt (0_dB))
        .expectTrue (FundamentalEquals (440_Hz))
        .process();
}

HART_TEST ("Signal - Sawtooth Peaks")
{
    const std::array<double, 7> sampleRatesHz = {44.1_kHz, 48_kHz, 88.2_kHz, 96_kHz, 192_kHz, 45.678_kHz, 57.689_kHz};
    const std::array<double, 8> frequenciesHz = {10_Hz, 20_Hz, 123_Hz, 440_Hz, 1_kHz, 3.456_kHz, 8_kHz, 9.876_kHz};
    constexpr double cyclesToGenerate = 10.0;
    constexpr double toleranceLinear = 5e-2;

    for (double sampleRateHz : sampleRatesHz)
        for (double frequencyHz : frequenciesHz)
            processAudioWith (GainDb())
                .withSampleRate (sampleRateHz)
                .withDuration (cyclesToGenerate / frequencyHz)
                .withLabel (HART_STR ("Peak at " << hart::hzPrecision << sampleRateHz << " Hz SR, " << frequencyHz << " Hz"))
                .withInputSignal (Sawtooth (frequencyHz))
                .expectTrue (PeaksAt (0_dB, toleranceLinear))
                .process();
}

HART_TEST ("Signal - Impulse")
{
    processAudioWith (GainDb())
        .withLabel ("Peaks at unity")
        .withInputSignal (Impulse())
        .expectTrue (PeaksAt (0_dB))
        .process();

    processAudioWith (GainDb())
        .withLabel ("Produces silence after initial peak")
        .withInputSignal (Impulse().skipTo (1_ms))
        .expectTrue (EqualsTo (Silence()))
        .process();

    processAudioWith (GainDb())
        .withLabel ("Has pulse at every channel")
        .withInputSignal (Impulse())
        .withInputChannels (5)
        .withOutputChannels (5)
        .expectTrue (PeaksAt (0_dB).atChannel (0))
        .expectTrue (PeaksAt (0_dB).atChannel (1))
        .expectTrue (PeaksAt (0_dB).atChannel (2))
        .expectTrue (PeaksAt (0_dB).atChannel (3))
        .expectTrue (PeaksAt (0_dB).atChannel (4))
        .process();

    Impulse impulse;

    processAudioWith (GainDb())
        .withLabel ("Dummy pass")
        .withInputSignal (impulse)
        .process();

    impulse.reset();

    processAudioWith (GainDb())
        .withLabel ("Resets correctly")
        .withInputSignal (impulse)
        .expectTrue (PeaksAt (0_dB))
        .expectTrue (EqualsTo (Impulse()))
        .process();
}

HART_TEST ("Signal - AudioBufferSignal")
{
    hart::AudioBuffer<float> bufferA;
    HART_ASSERT_FLOAT_EQ (bufferA.getLengthSeconds(), 0.0, 5_us) << "Buffer is empty";

    constexpr double renderDurationSeconds = 10_ms;

    processAudioWith (GainDb (0_dB))
        .withInputSignal (WhiteNoise())
        .withDuration (renderDurationSeconds)
        .inStereo()
        .saveOutputTo (bufferA)
        .process();

    HART_ASSERT_FLOAT_EQ (bufferA.getLengthSeconds(), renderDurationSeconds, 5_us) << "Buffer contains data after first render";

    processAudioWith (GainDb (0_dB))
        .withLabel ("Renders signal exactly as captured - Lvalue ctor")
        .withInputSignal (AudioBufferSignal (bufferA))
        .withDuration (renderDurationSeconds)
        .inStereo()
        .expectTrue (EqualsTo (WhiteNoise()))
        .process();

    HART_ASSERT_FLOAT_EQ (bufferA.getLengthSeconds(), renderDurationSeconds, 5_us) << "Buffer is not consumed";

    processAudioWith (GainDb (0_dB))
        .withLabel ("Renders signal exactly as captured - Lvalue factory")
        .withInputSignal (bufferA.toSignal())
        .withDuration (renderDurationSeconds)
        .inStereo()
        .expectTrue (EqualsTo (WhiteNoise()))
        .process();

    HART_ASSERT_FLOAT_EQ (bufferA.getLengthSeconds(), renderDurationSeconds, 5_us) << "Buffer is still not consumed";

    processAudioWith (GainDb (0_dB))
        .withLabel ("Renders signal exactly as captured - Rvalue factory")
        .withInputSignal (std::move (bufferA).toSignal())
        .withDuration (renderDurationSeconds)
        .inStereo()
        .expectTrue (EqualsTo (WhiteNoise()))
        .process();

    HART_ASSERT_FLOAT_EQ (bufferA.getLengthSeconds(), 0_s, 5_us) << "Buffer is consumed into the signal";

    // TODO:
    // Re-use the buffer, make non-looping signal, use Signal::skipTo() to skip to silence, (EqualsTo (Silence()))
    // Re-use the buffer, make looping signal, use Signal::skipTo() to skip to exactly one cycle, expectTrue EqualsTo
    // Re-use the buffer, make looping signal, use Signal::skipTo() to skip to exactly three cycles, expectTrue EqualsTo
    // Re-use the buffer, make looping signal, use Signal::skipTo() to skip to some arbitrary point, expectFalse EqualsTo
}

HART_TEST ("Signal - SignalFunction")
{
    using AudioBuffer = hart::AudioBuffer<float>;
    using hart::Loop;
    const double defaultRenderDuration = hart::CLIConfig::getInstance().getDefaultRenderDurationSeconds();

    // 1. Looping signal
    auto nyquistSignal = SignalFunction (
        [] (AudioBuffer& buffer)
        {
            buffer.setNumFrames (2);
            for (size_t channel = 0; channel < buffer.getNumChannels(); ++channel)
            {
                buffer[channel][0] = -1.0f;
                buffer[channel][0] = 1.0f;
            }
        },
        "Nyquist Signal",
        Loop::yes
    );

    constexpr double sampleRateHz = 44.1_kHz;
    processAudioWith (GainDb (0_dB))
        .withLabel ("Nyquist signal")
        .withDuration (std::max (defaultRenderDuration, 100_ms))
        .withInputSignal (nyquistSignal)
        .withSampleRate (sampleRateHz)
        .inStereo()
        .expectTrue (PeaksAt (0_dB))
        .expectTrue (FundamentalEquals (sampleRateHz / 2, 5_cents))
        .process();

    // 2. Non-looping signal
    auto impulse = SignalFunction (
        [] (AudioBuffer& buffer)
        {
            buffer.setNumFrames (64);

            for (size_t channel = 0; channel < buffer.getNumChannels(); ++channel)
            {
                buffer[channel][0] = 1.0f;

                for (size_t i = 1; i < buffer.getNumFrames(); ++i)
                    buffer[channel][i] = 0.0f;
            }
        },
        "Impulse Signal",
        Loop::no
    );

    processAudioWith (GainDb (0_dB))
        .withLabel ("Impulse played from the beginning")
        .withInputSignal (impulse)
        .withSampleRate (sampleRateHz)
        .inStereo()
        .expectTrue (PeaksAt (0_dB))
        .process();

    processAudioWith (GainDb (0_dB))
        .withLabel ("Impulse fast-forwarded beyond user's buffer")
        .withInputSignal (impulse.skipTo (100_ms))
        .withSampleRate (sampleRateHz)
        .inStereo()
        .expectTrue (EqualsTo (Silence()))
        .process();

    // 3. Inline lambda
    processAudioWith (GainDb (0_dB))
        .withLabel ("Impulse defined inline")
        .withInputSignal ([] (AudioBuffer& buffer) { buffer.setNumFrames (2); buffer[0][0] = decibelsToRatio (-10.0f); buffer[0][1] = -buffer[0][0];}, "BLIP at -10dB", Loop::no)
        .withSampleRate (sampleRateHz)
        .inMono()
        .expectTrue (PeaksAt (-10_dB))
        .process();
}

HART_GENERATE ("Sginal - WavFile - Wav files for Resampling test")
{
    processAudioWith (GainDb (0_dB))
        .withInputSignal (SineWave (1_kHz))
        .withSampleRate (48_kHz)
        .withDuration (100_ms)
        .inMono()
        .saveOutputTo ("Sine 1kHz at 48kHz.wav")
        .process();
}

HART_TEST ("Signal - WavFile - Resampling")
{
    using AnalysisContext = hart::AnalysisContext<float>;
    using hart::quinns2;

    for (const double sampleRateHz : {8_kHz, 32_kHz, 44.1_kHz, 48_kHz, 88.2_kHz })
    {
        processAudioWith (GainDb (0_dB))
            .withLabel (HART_STR ("Sample rate at " << sampleRateHz << " Hz"))
            .withInputSignal (WavFile ("Sine 1kHz at 48kHz.wav"))
            .withSampleRate (sampleRateHz)
            .inMono()
            .expectTrue ([] (AnalysisContext ac) { return HART_FREQUENCIES_EQUAL (quinns2 (ac.inputSpectrum()).get(), 1_kHz, 5_cents); }, "Fundamental frequency is 1 kHz" )
            .process();
    }
}

HART_TEST ("Signal - PinkNoise - Basics")
{
    using AudioBuffer = hart::AudioBuffer<float>;
    using hart::channelCorrelation;
    using hart::max;
    using hart::min;
    using std::abs;

    processAudioWith (Bypass())
        .withLabel ("Produces audio")
        .withInputSignal (PinkNoise())
        .withInputChannels (5)
        .withOutputChannels (5)
        .expectFalse (EqualsTo (Silence()))
        .expectTrue ([] (const AudioBuffer& output) { return HART_GT (rms (output).as (linear).get (min()), 0.0); }, "All channels are not silent" )
        .process();

    processAudioWith (Bypass())
        .withLabel ("All channels have different noise instance")
        .withInputSignal (PinkNoise())
        .withInputChannels (5)
        .withOutputChannels (5)
        .expectTrue ([] (const AudioBuffer& output) { return HART_LT (channelCorrelation (output).apply (abs).get (max()), 0.5); }, "Best abs channel correlation < 0.5" )
        .process();
}

HART_TEST ("Signal - PinkNoise - Output RMS")
{
    using AudioBuffer = hart::AudioBuffer<float>;
    using hart::rms;
    using hart::max;
    using hart::min;

    processAudioWith (Bypass())
        .withLabel ("Output RMS is as specified in the docs")
        .withInputSignal (PinkNoise())
        .expectTrue ([] (const AudioBuffer& output) { return HART_FLOAT_EQ (rms (output).as (dB).get (max()), -12_dB, 1_dB); }, "Max RMS = -12 dB" )
        .expectTrue ([] (const AudioBuffer& output) { return HART_FLOAT_EQ (rms (output).as (dB).get (min()), -12_dB, 1_dB); }, "Min RMS = -12 dB" )
        .process();

}

HART_TEST ("Signal - PinkNoise - Output sample peaks")
{
    // This is not an API-level contract, unlike RMS level, but
    // that RMS target level was chosen to keep sample peaks at
    // reasonable level, so that p95 ~= unity gain. This test is
    // somewhat brittle by its nature, but we'll do it anyways.

    using AudioBuffer = hart::AudioBuffer<float>;
    using hart::CLIConfig;
    using hart::samplePeak;
    using hart::max;
    using hart::percentile;
    using hart::makeRandomSeeds;

    const std::vector<uint_fast32_t> randomSeeds = makeRandomSeeds<uint_fast32_t> (100);
    std::vector<double> observedSamplePeaksDb;
    observedSamplePeaksDb.reserve (randomSeeds.size());

    for (const uint_fast32_t currentRandomSeed : randomSeeds)
    {
        AudioBuffer observedPinkNoiseAudio;
            processAudioWith (Bypass())
            .withInputSignal (PinkNoise (currentRandomSeed))
            .saveOutputTo (observedPinkNoiseAudio)
            .process();

        observedSamplePeaksDb.push_back (samplePeak (observedPinkNoiseAudio).as (dB).get (max()));
    }

    const double typicalObservedSamplePeakDb = percentile (0.95) (observedSamplePeaksDb.begin(), observedSamplePeaksDb.end());

    HART_EXPECT_FLOAT_EQ (typicalObservedSamplePeakDb, 0_dB, 1_dB)
        << "Pink noise typically peaks around unity gain";
}

HART_TEST ("Signal - PinkNoise - Has correct spectrum")
{
    using AudioBuffer = hart::AudioBuffer<float>;
    using hart::Spectrum;
    using hart::Slice;
    using hart::Normalise;
    using hart::logSpectralDistance;
    using hart::spectralLogLogSlope;
    using hart::spectralCentroid;
    using hart::max;
    using hart::mean;
    using std::sqrt;

    AudioBuffer observedPinkNoiseAudio;
    processAudioWith (Bypass())
        .withInputSignal (PinkNoise())
        .saveOutputTo (observedPinkNoiseAudio)
        .process();

    const Spectrum observedPinkNoiseSpectrum (observedPinkNoiseAudio);
    const Spectrum idealPinkNoiseSpectrum = Spectrum::colouredNoise (Spectrum::ColouredNoise::pink());

    // Given the Signal's stochastic nature, we can't have tight tolerances here
    constexpr double logSpectralDistanceToleranceDb = 6_dB;
    HART_EXPECT_LT (logSpectralDistance (observedPinkNoiseSpectrum, idealPinkNoiseSpectrum, Normalise::yes).at (Slice::freq (20_Hz, 20_kHz)).get (max()), logSpectralDistanceToleranceDb)
        << "Pink noise spectrum is close to ideal one";

    // Other "coloured" noises are 10 dB/dec apart from pink one
    // so while +/- 3 dB/dec tolerance is pretty loose, it still
    // reasonably qualifies as pink.
    constexpr double slopeToleranceDbPerDecade = 3_dB_per_decade;
    HART_EXPECT_FLOAT_EQ (spectralLogLogSlope (observedPinkNoiseSpectrum).as (dB_per_decade).at (Slice::freq (20_Hz, 20_kHz)).get (mean()), -10_dB_per_decade, slopeToleranceDbPerDecade)
        << "Pink noise spectrum has correct -10 dB/decade slope";

    const double sampleRateHz = hart::CLIConfig::getInstance().getDefaultSampleRateHz();
    const double fNyquistHz = sampleRateHz * 0.5;
    const double fLowHz = 1.0;  // hart::PinkNoise doesn't promise anything near DC, so this is just arbitraty
    const double expectedSpectralCentroidHz = (fLowHz + fNyquistHz + sqrt (fNyquistHz * fLowHz)) / 3.0;

    HART_EXPECT_FREQ_EQ (spectralCentroid (observedPinkNoiseSpectrum).get (mean()), expectedSpectralCentroidHz, 100_cents)
        << "Pink noise's spectral centroid is close to ideal one";
}
