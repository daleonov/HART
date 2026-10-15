#include <algorithm>  // max()
#include <cmath>  // abs(), isnan(), isinf(), cos(), acos(), tan()

#include "hart.hpp"
#include "exponential_decay.hpp"
#include "unstable_decay.hpp"

HART_DECLARE_ALIASES_FOR_FLOAT;
HART_DECLARE_ALIASES_FOR_UNITS;
using AudioBuffer = hart::AudioBuffer<float>;
using hart::floatsEqual;

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

HART_PARAMETRISED_TEST ("Metrics - Jacobsen's Estimator - getCandan1Correction() helper")
{
    constexpr size_t largestPowerOfTwoOnThisPlatform = std::numeric_limits<size_t>::digits - 1;

    const size_t fftSize = HART_GENERATE_VALUE (
        size_t (1) << 5,
        size_t (1) << 10,
        size_t (1) << 15,
        size_t (1) << largestPowerOfTwoOnThisPlatform
    );

    const double n = static_cast<double> (fftSize);
    const double piOverN = hart::pi / n;
    const double expectedValue = std::tan (piOverN) / (piOverN);
    const double observedValue = hart::Jacobsen::getCandan1Correction (fftSize);

    HART_EXPECT_FLOAT_EQ (expectedValue, observedValue, 1e-16);
}

HART_PARAMETRISED_TEST ("Metrics - Jacobsen's Estimator - Vanilla version")
{
    using AnalysisContext = hart::AnalysisContext<float>;
    using hart::jacobsen;

    const double expectedFundamentalHz = HART_GENERATE_VALUE (123_Hz, 456_Hz, 1_kHz, 5_kHz, 15_kHz);
    HART_CAPTURE_VALUE (expectedFundamentalHz);

    processAudioWith (Bypass())
        .withInputSignal (SineWave (expectedFundamentalHz))
        .inMono()
        .expectTrue (
            [expectedFundamentalHz] (AnalysisContext ac)
            {
                return HART_FREQUENCIES_EQUAL (jacobsen (ac.outputSpectrum()).get(), expectedFundamentalHz, 10_cents);
            },
            "Estimated sine wave frequency ~= Actual frequency"
        )
        .process();

    processAudioWith (Bypass())
        .withInputSignal (Sawtooth (expectedFundamentalHz))
        .inMono()
        .expectTrue (
            [expectedFundamentalHz] (AnalysisContext ac)
            {
                return HART_FREQUENCIES_EQUAL (jacobsen (ac.outputSpectrum()).get(), expectedFundamentalHz, 10_cents);
            },
            "Estimated sawtooth frequency ~= Actual frequency"
        )
        .process();
}

HART_PARAMETRISED_TEST ("Metrics - Jacobsen's Estimator - Candan's corrections")
{
    using std::abs;
    using hart::Spectrum;
    using hart::jacobsen;
    using hart::ratioToCents;
    using namespace hart::Jacobsen;

    const double expectedFundamentalHz = HART_GENERATE_VALUE (123_Hz, 456_Hz, 1_kHz, 5_kHz, 15_kHz);
    HART_CAPTURE_VALUE (expectedFundamentalHz);

    AudioBuffer audioBuffer;
    processAudioWith (Bypass())
        .withInputSignal (SineWave (expectedFundamentalHz))
        .inMono()
        .saveOutputTo (audioBuffer)
        .process();

    Spectrum spectrum (audioBuffer);
    const double jacobsenEstimationHz = jacobsen (spectrum, Correction::none);
    const double candan1EstimationHz = jacobsen (spectrum, Correction::candan1);
    const double candan2EstimationHz = jacobsen (spectrum, Correction::candan2);

    HART_CAPTURE_VALUE (jacobsenEstimationHz);
    HART_CAPTURE_VALUE (candan1EstimationHz);
    HART_CAPTURE_VALUE (candan2EstimationHz);

    const double jacobsenErrorCents = abs (ratioToCents (jacobsenEstimationHz / expectedFundamentalHz));
    const double candan1ErrorCents = abs (ratioToCents (candan1EstimationHz / expectedFundamentalHz));
    const double candan2ErrorCents = abs (ratioToCents (candan2EstimationHz / expectedFundamentalHz));

    HART_CAPTURE_VALUE (jacobsenErrorCents);
    HART_CAPTURE_VALUE (candan1ErrorCents);
    HART_CAPTURE_VALUE (candan2ErrorCents);

    // They're expected to be numerically slightly different
    // (Unless it's a very long FFT)
    HART_EXPECT_FLOAT_NE (jacobsenEstimationHz, candan1EstimationHz, 1e-16);
    HART_EXPECT_FLOAT_NE (candan1EstimationHz, candan2EstimationHz, 1e-16);

    // They all estimate the same frequency
    HART_EXPECT_FREQ_EQ (jacobsenEstimationHz, expectedFundamentalHz, 10_cents);
    HART_EXPECT_FREQ_EQ (candan1EstimationHz, expectedFundamentalHz, 10_cents);
    HART_EXPECT_FREQ_EQ (candan2EstimationHz, expectedFundamentalHz, 10_cents);

    // Each consecutive correction is statistically better than previous
    // one, but we're not testing it as a part of the metric's contract.
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

