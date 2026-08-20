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
    const hart::CLIConfig& cliConfig = hart::CLIConfig::getInstance();
    const double sampleRateHz = cliConfig.getDefaultSampleRateHz();
    const double renderDurationSeconds = cliConfig.getDefaultRenderDurationSeconds();
    const size_t numFrames = hart::roundToSizeT (renderDurationSeconds * sampleRateHz);
    const size_t numChannels = cliConfig.getDefaultNumInputChannels();

    const hart::Spectrum spectrum = hart::Spectrum::zeros (numChannels, numFrames, sampleRateHz);
    const AudioBuffer buffer = spectrum.toAudioBuffer<float>();

    processAudioWith (GainDb (0_dB))
        .withInputSignal (AudioBufferSignal (std::move (buffer)))
        .assertTrue (EqualsTo (Silence()))
        .process();
}
