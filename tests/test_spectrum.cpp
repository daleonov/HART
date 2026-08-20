#include "hart.hpp"

HART_DECLARE_ALIASES_FOR_FLOAT;
using AudioBuffer = hart::AudioBuffer<float>;

HART_TEST ("Spectrum - Converts to AudioBuffer correctly")
{
    AudioBuffer bufferA;
    processAudioWith (GainDb (0_dB))
        .withInputSignal (WhiteNoise())
        .saveOutputTo (bufferA)
        .process();

    const hart::Spectrum spectrum (bufferA);
    const AudioBuffer bufferB = spectrum.toAudioBuffer<float>();

    HART_EXPECT_EQ (bufferA, bufferB);
}

HART_TEST ("Spectrum - Spectrum::zeros() converts to a silent AudioBuffer")
{
    const hart::Spectrum spectrum = hart::Spectrum::zeros();
    const AudioBuffer buffer = spectrum.toAudioBuffer<float>();

    processAudioWith (GainDb (0_dB))
        .withInputSignal (AudioBufferSignal (std::move (buffer)))
        .assertTrue (EqualsTo (Silence()))
        .process();
}

HART_TEST ("Spectrum - Spectrum::colouredNoise() creates ideal magnitudes")
{
    constexpr double lowCutoffFrequencyHz = 20_Hz;

    const hart::Spectrum whiteNoiseSpectrum = hart::Spectrum::colouredNoise (0.0, lowCutoffFrequencyHz);
    const hart::Spectrum pinkNoiseSpectrum = hart::Spectrum::colouredNoise (-1.0, lowCutoffFrequencyHz);
    const hart::Spectrum brownNoiseSpectrum = hart::Spectrum::colouredNoise (-2.0, lowCutoffFrequencyHz);

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
    constexpr double beta = -1.0;
    constexpr double lowCutoffFrequencyHz = 20_Hz;

    const hart::Spectrum spectrumA = hart::Spectrum::colouredNoise (beta, 123, lowCutoffFrequencyHz);
    const hart::Spectrum spectrumB = hart::Spectrum::colouredNoise (beta, 123, lowCutoffFrequencyHz);
    const hart::Spectrum spectrumC = hart::Spectrum::colouredNoise (beta, 456, lowCutoffFrequencyHz);

    const size_t numChannels = spectrumA.getNumChannels();
    const size_t cutoffBin = spectrumA.findClosestBin (lowCutoffFrequencyHz);

    // TODO: Implement "==" for a pair of spectra
    for (size_t channel = 0; channel < numChannels; ++channel)
    {
        for (size_t bin = cutoffBin; bin < spectrumA.getNumBins(); ++bin)
        {
            const std::complex<double> binA = spectrumA.getBinValue (channel, bin);
            const std::complex<double> binB = spectrumB.getBinValue (channel, bin);
            const std::complex<double> binC = spectrumC.getBinValue (channel, bin);
            HART_EXPECT_FLOAT_EQ (std::real (binA), std::real (binB), 1e-16);
            HART_EXPECT_FLOAT_EQ (std::imag (binA), std::imag (binB), 1e-16);
            HART_EXPECT_FALSE (std::real (binA) == std::real (binC) && std::imag (binA) == std::imag (binC));
        }
    }
}

HART_TEST ("Spectrum - Spectrum::colouredNoise() converts to non-silent AudioBuffer")
{
    const hart::Spectrum spectrum = hart::Spectrum::colouredNoise (-1.0, 20.0);

    processAudioWith (GainDb (0_dB))
        .withInputSignal (AudioBufferSignal (spectrum.toAudioBuffer<float>()))
        .expectFalse (EqualsTo (Silence()))
        .process();
}
