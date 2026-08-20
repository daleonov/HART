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
