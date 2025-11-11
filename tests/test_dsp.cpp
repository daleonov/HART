#include "hart.hpp"
#include "hart_matchers.hpp"

HART_TEST ("Gain - Gain Values")
{
    hart::processAudioWith (hart::Gain<float>())
        .withInputSignal (hart::SineWave<float>())
        .withSampleRate (44100.0)
        .withBlockSize (1024)
        .withDuration (0.1)
        .inMono()
        .withValue (hart::Gain<float>::gainDb, -oo_dB)
        .expectTrue (hart::equalsTo (hart::Silence<float>()))
        .process();

    hart::processAudioWith (hart::Gain<float>())
        .withInputSignal (hart::SineWave<float>())
        .withSampleRate (44100.0)
        .withBlockSize (1024)
        .withDuration (0.1)
        .inMono()
        .withValue (hart::Gain<float>::gainDb, 0_dB)
        .expectTrue (hart::equalsTo (hart::SineWave<float>()))
        .process();

    hart::processAudioWith (hart::Gain<float>())
        .withInputSignal (hart::SineWave<float>())
        .withSampleRate (44100.0)
        .withBlockSize (1024)
        .withDuration (0.1)
        .inMono()
        .withValue (hart::Gain<float>::gainDb, -3_dB)
        .expectTrue (hart::PeaksBelow<float> (-2.99_dB))
        .expectFalse (hart::PeaksBelow<float> (-3.01_dB))
        .process();
}
