#include "hart.hpp"

HART_DECLARE_ALIASES_FOR_FLOAT;

HART_TEST ("Detecting denormals")
{
    processAudioWith (GainDb())
        .withLabel ("Unity gain")
        .withInputSignal (SineWave())
        .expectTrue (NoDenormals())
        .process();

    processAudioWith (GainLinear (1e-38))
        .withLabel ("-760 dB - All denormals")
        .withInputSignal (SineWave())
        .expectFalse (NoDenormals())
        .process();

    processAudioWith (GainLinear (1e-30))
        .withLabel ("-600 dB - Some denormals")
        .withInputSignal (SineWave())
        .expectFalse (NoDenormals())
        .expectFalse (EqualsTo (Silence(), 1e-36))
        .process();

    processAudioWith (GainLinear (1e-25))
        .withLabel ("-500 dB - Occasional denormals")
        .withInputSignal (SineWave())
        .expectFalse (NoDenormals())
        .expectFalse (EqualsTo (Silence(), 1e-36))
        .process();

    processAudioWith (GainLinear (0))
        .withLabel ("Complete silence")
        .withInputSignal (SineWave())
        .expectTrue (NoDenormals())
        .expectTrue (EqualsTo (Silence(), 1e-36))
        .process();
}
