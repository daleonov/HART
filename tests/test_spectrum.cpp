#include <array>

#include "hart.hpp"

HART_DECLARE_ALIASES_FOR_FLOAT;
HART_DECLARE_ALIASES_FOR_UNITS;
using AudioBuffer = hart::AudioBuffer<float>;
using hart::Spectrum;

HART_TEST ("Spectrum - Converts to AudioBuffer correctly")
{
    AudioBuffer bufferA;
    processAudioWith (GainDb (0_dB))
        .withInputSignal (WhiteNoise())
        .saveOutputTo (bufferA)
        .process();

    const Spectrum spectrum (bufferA);
    const AudioBuffer bufferB = spectrum.toAudioBuffer<float>();

    HART_EXPECT_EQ (bufferA, bufferB);
}

HART_TEST ("Spectrum - Spectrum::zeros() converts to a silent AudioBuffer")
{
    const Spectrum spectrum = Spectrum::zeros();
    const AudioBuffer buffer = spectrum.toAudioBuffer<float>();

    processAudioWith (GainDb (0_dB))
        .withInputSignal (AudioBufferSignal (std::move (buffer)))
        .assertTrue (EqualsTo (Silence()))
        .process();
}

HART_TEST ("Spectrum - Spectrum::colouredNoise() has correct RMS at power-of-two sizes")
{
    using hart::rms;
    using hart::mean;

    const std::array<Spectrum, 7> spectra = {{
        Spectrum::colouredNoise (Spectrum::ColouredNoise::white().withDuration (1 << 12)),
        Spectrum::colouredNoise (Spectrum::ColouredNoise::pink().withDuration (1 << 16)),
        Spectrum::colouredNoise (Spectrum::ColouredNoise::brown().withDuration (1 << 15)),
        Spectrum::colouredNoise (Spectrum::ColouredNoise::blue().withDuration (1 << 18)),
        Spectrum::colouredNoise (Spectrum::ColouredNoise::violet().withDuration (1 << 10)),
        Spectrum::colouredNoise (Spectrum::ColouredNoise::pink().withDuration (1 << 12).withLowCutoff (500_Hz)),
        Spectrum::colouredNoise (Spectrum::ColouredNoise::pink().withDuration (1 << 12).withLowCutoff (5_Hz))
    }};

    for (const Spectrum& spectrum : spectra)
    {
        HART_EXPECT_FLOAT_EQ (rms (spectrum). as (dB). get (mean()), -12_dB, 1e-8)
            << "RMS is 12 dB, when calculated in frequency domain";
    
        HART_EXPECT_FLOAT_EQ (rms (spectrum.toAudioBuffer<float>()). as (dB). get (mean()), -12_dB, 0.01_dB)
            << "RMS is still 12 dB, when converted to time domain";
    }
}

HART_TEST ("Spectrum - Spectrum::colouredNoise() has correct RMS at arbitrary sizes")
{
    using hart::rms;
    using hart::max;

    // Worst case would be (power of two) + 1 size, plus
    // some unfortunate random phase distribution.
    // Latter is not tested herer, as it's hard to produce.

    constexpr double sqrt2 = 1.414213562373095;
    constexpr double expectedRmsAtPowerOfTwoSizeDb = -12_dB;
    const double worstCaseRmsDeviationDb = hart::ratioToDecibels (sqrt2);  // ~3 dB
    const double expectedUpperRmsLimitDb = expectedRmsAtPowerOfTwoSizeDb + worstCaseRmsDeviationDb;

    const std::array<Spectrum, 7> spectra = {{
        Spectrum::colouredNoise (Spectrum::ColouredNoise::white().withDuration (4410)),
        Spectrum::colouredNoise (Spectrum::ColouredNoise::pink().withDuration (12345)),
        Spectrum::colouredNoise (Spectrum::ColouredNoise::brown().withDuration (100000)),
        Spectrum::colouredNoise (Spectrum::ColouredNoise::blue().withDuration ((1 << 15) + 1)),
        Spectrum::colouredNoise (Spectrum::ColouredNoise::violet().withDuration ((1 << 10) + 1)),
        Spectrum::colouredNoise (Spectrum::ColouredNoise::pink().withDuration ((1 << 12) + 1).withLowCutoff (500_Hz)),
        Spectrum::colouredNoise (Spectrum::ColouredNoise::pink().withDuration ((1 << 12) + 1).withLowCutoff (5_Hz))
    }};
    
    for (const Spectrum& spectrum : spectra)
    {
        HART_ASSERT_FLOAT_EQ (rms (spectrum). as(dB). get (max()), -12_dB, 1e-8)
            << "RMS is 12 dB, when calculated in frequency domain, even for worst-case sizes";

        HART_EXPECT_LE (rms (spectrum.toAudioBuffer<float>()). as (dB). get (max()), expectedUpperRmsLimitDb)
            << "RMS < 9 dB, when calculated in time domain, for non-power-of-two duratrions";
    }
}

HART_TEST ("Spectrum - Spectrum::colouredNoise() creates ideal magnitudes")
{
    constexpr double lowCutoffFrequencyHz = 20_Hz;

    const Spectrum whiteNoiseSpectrum = Spectrum::colouredNoise (Spectrum::ColouredNoise::white().withLowCutoff (lowCutoffFrequencyHz));
    const Spectrum pinkNoiseSpectrum = Spectrum::colouredNoise (Spectrum::ColouredNoise::pink().withLowCutoff (lowCutoffFrequencyHz));
    const Spectrum brownNoiseSpectrum = Spectrum::colouredNoise (Spectrum::ColouredNoise::brown().withLowCutoff (lowCutoffFrequencyHz));

    const size_t numChannels = whiteNoiseSpectrum.getNumChannels();
    const size_t firstBin = static_cast<size_t> (std::ceil (lowCutoffFrequencyHz / whiteNoiseSpectrum.getBinWidthHz()));

    const double firstBinFrequencyHz = whiteNoiseSpectrum.getBinFrequencyHz (firstBin);
    HART_ASSERT_FLOAT_NE (firstBinFrequencyHz, 0_Hz, 1e-16);

    for (size_t channel = 0; channel < numChannels; ++channel)
    {

        for (size_t bin = 0; bin < firstBin; ++bin)
        {
            // Expecting zeros in all bins under cutoff frequency
            HART_EXPECT_FLOAT_EQ (whiteNoiseSpectrum.getBinMagnitude (channel, bin), 0.0, 1e-12);
            HART_EXPECT_FLOAT_EQ (pinkNoiseSpectrum.getBinMagnitude (channel, bin), 0.0, 1e-12);
            HART_EXPECT_FLOAT_EQ (brownNoiseSpectrum.getBinMagnitude (channel, bin), 0.0, 1e-12);
        }

        const double whiteNoiseA0 = whiteNoiseSpectrum.getBinMagnitude (channel, firstBin);
        const double pinkNoiseA0 = pinkNoiseSpectrum.getBinMagnitude (channel, firstBin);
        const double brownNoiseA0 = brownNoiseSpectrum.getBinMagnitude (channel, firstBin);

        for (size_t bin = firstBin; bin < whiteNoiseSpectrum.getNumBins(); ++bin)
        {
            const double frequencyHz = whiteNoiseSpectrum.getBinFrequencyHz (bin);

            HART_EXPECT_FLOAT_EQ (whiteNoiseSpectrum.getBinMagnitude (channel, bin), whiteNoiseA0, 1e-12);
            HART_EXPECT_FLOAT_EQ (pinkNoiseSpectrum.getBinMagnitude (channel, bin), pinkNoiseA0 * std::pow (frequencyHz / firstBinFrequencyHz, -0.5), 1e-12);
            HART_EXPECT_FLOAT_EQ (brownNoiseSpectrum.getBinMagnitude (channel, bin), brownNoiseA0 * firstBinFrequencyHz / frequencyHz, 1e-12);
        }
    }
}

HART_TEST ("Spectrum - Spectrum::colouredNoise() is deterministic")
{
    constexpr double lowCutoffFrequencyHz = 20_Hz;

    const Spectrum spectrumA = Spectrum::colouredNoise (Spectrum::ColouredNoise::pink().withRandomSeed (123).withLowCutoff (lowCutoffFrequencyHz));
    const Spectrum spectrumB = Spectrum::colouredNoise (Spectrum::ColouredNoise::pink().withRandomSeed (123).withLowCutoff (lowCutoffFrequencyHz));
    const Spectrum spectrumC = Spectrum::colouredNoise (Spectrum::ColouredNoise::pink().withRandomSeed (456).withLowCutoff (lowCutoffFrequencyHz));

    const size_t numChannels = spectrumA.getNumChannels();
    const size_t cutoffBin = spectrumA.findClosestBin (lowCutoffFrequencyHz);

    HART_EXPECT_EQ (spectrumA, spectrumB);
    HART_EXPECT_NE (spectrumA, spectrumC);
}

HART_TEST ("Spectrum - Spectrum::colouredNoise() converts to non-silent AudioBuffer")
{
    const Spectrum spectrum = Spectrum::colouredNoise (Spectrum::ColouredNoise::pink());

    processAudioWith (GainDb (0_dB))
        .withInputSignal (AudioBufferSignal (spectrum.toAudioBuffer<float>()))
        .expectFalse (EqualsTo (Silence()))
        .process();
}

HART_TEST ("Spectrum - Multiply operation")
{
    const Spectrum spectrumA = Spectrum::colouredNoise (Spectrum::ColouredNoise::pink());
    const Spectrum spectrumB = spectrumA * 2.0;

    HART_EXPECT_NE (spectrumA, spectrumB);
    HART_EXPECT_EQ (spectrumA, spectrumB * 0.5);

    HART_EXPECT_EQ (spectrumA * 0.0, Spectrum::zeros());
    HART_EXPECT_EQ (spectrumA * 1.0, spectrumA);

    const Spectrum spectrumC = spectrumA * (-1.0);
    HART_EXPECT_NE (spectrumA, spectrumC);
    HART_EXPECT_EQ (spectrumA, spectrumC * (-1.0));

    HART_EXPECT_EQ (((spectrumA * 0.2) * 12.0), (spectrumA * (-8.0)) * (-0.3));
}
