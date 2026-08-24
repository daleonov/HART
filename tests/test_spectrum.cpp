#include "hart.hpp"

HART_DECLARE_ALIASES_FOR_FLOAT;
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

HART_TEST ("Spectrum - Spectrum::colouredNoise() creates ideal magnitudes")
{
    constexpr double lowCutoffFrequencyHz = 20_Hz;

    const Spectrum whiteNoiseSpectrum = Spectrum::colouredNoise (Spectrum::ColouredNoise::white().withLowCutoff (lowCutoffFrequencyHz));
    const Spectrum pinkNoiseSpectrum = Spectrum::colouredNoise (Spectrum::ColouredNoise::pink().withLowCutoff (lowCutoffFrequencyHz));
    const Spectrum brownNoiseSpectrum = Spectrum::colouredNoise (Spectrum::ColouredNoise::brown().withLowCutoff (lowCutoffFrequencyHz));

    const size_t numChannels = whiteNoiseSpectrum.getNumChannels();
    const size_t cutoffBin = whiteNoiseSpectrum.findClosestBin (lowCutoffFrequencyHz);

    for (size_t channel = 0; channel < numChannels; ++channel)
    {
        for (size_t bin = cutoffBin; bin < whiteNoiseSpectrum.getNumBins(); ++bin)
        {
            const double frequencyHz = whiteNoiseSpectrum.getBinFrequencyHz (bin);

            HART_EXPECT_FLOAT_EQ (whiteNoiseSpectrum.getBinMagnitude (channel, bin), 1.0, 1e-12);
            HART_EXPECT_FLOAT_EQ (pinkNoiseSpectrum.getBinMagnitude (channel, bin), std::pow (frequencyHz / lowCutoffFrequencyHz, -0.5), 1e-12);
            HART_EXPECT_FLOAT_EQ (brownNoiseSpectrum.getBinMagnitude (channel, bin), lowCutoffFrequencyHz / frequencyHz, 1e-12);
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
