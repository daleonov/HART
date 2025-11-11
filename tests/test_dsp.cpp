#include "hart.hpp"
#include "hart_matchers.hpp"

using hart::equalsTo;
using hart::HardClip;
using hart::Gain;
using hart::PeaksAt;
using hart::processAudioWith;
using hart::Silence;
using hart::SineWave;

HART_TEST ("Gain - Gain Values")
{
    processAudioWith (Gain<float>())
        .withInputSignal (SineWave<float>())
        .withValue (Gain<float>::gainDb, -oo_dB)
        .expectTrue (equalsTo (Silence<float>()))
        .process();

    processAudioWith (Gain<float>())
        .withInputSignal (SineWave<float>())
        .withValue (Gain<float>::gainDb, 0_dB)
        .expectTrue (equalsTo (SineWave<float>()))
        .process();

    processAudioWith (Gain<float>())
        .withInputSignal (SineWave<float>())
        .withValue (Gain<float>::gainDb, -3_dB)
        .expectTrue (PeaksAt<float> (-3_dB))
        .process();

    processAudioWith (Gain<float>())
        .withInputSignal (SineWave<float>())
        .withValue (Gain<float>::gainDb, -20_dB)
        .expectTrue (PeaksAt<float> (-20_dB))
        .process();

    processAudioWith (Gain<float>())
        .withInputSignal (SineWave<float>())
        .withValue (Gain<float>::gainDb, +6_dB)
        .expectTrue (PeaksAt<float> (+6_dB))
        .process();
}

HART_TEST ("Gain - Channel Layouts")
{
    processAudioWith (Gain<float>())
        .withInputSignal (SineWave<float>())
        .inMono()
        .withValue (Gain<float>::gainDb, -3_dB)
        .expectTrue (PeaksAt<float> (-3_dB))
        .process();

    processAudioWith (Gain<float>())
        .withInputSignal (SineWave<float>())
        .inStereo()
        .withValue (Gain<float>::gainDb, -3_dB)
        .expectTrue (PeaksAt<float> (-3_dB))
        .process();

    processAudioWith (Gain<float>())
        .withInputSignal (SineWave<float>())
        .withMonoInput()
        .withStereoOutput()
        .withValue (Gain<float>::gainDb, -3_dB)
        .expectTrue (PeaksAt<float> (-3_dB))
        .process();

    processAudioWith (Gain<float>())
        .withInputSignal (SineWave<float>())
        .withMonoInput()
        .withOutputChannels (5)
        .withValue (Gain<float>::gainDb, -3_dB)
        .expectTrue (PeaksAt<float> (-3_dB))
        .process();
}

HART_TEST ("HardClip - Threshold Values")
{
    processAudioWith (HardClip<float>())
        .withInputSignal (SineWave<float>())
        .withValue (HardClip<float>::thresholdDb, -oo_dB)
        .expectTrue (equalsTo (Silence<float>()))
        .process();

    processAudioWith (HardClip<float>())
        .withInputSignal (SineWave<float>())
        .withValue (HardClip<float>::thresholdDb, 0_dB)
        .expectTrue (equalsTo (SineWave<float>()))
        .process();

    processAudioWith (HardClip<float>())
        .withInputSignal (SineWave<float>())
        .withValue (HardClip<float>::thresholdDb, -3_dB)
        .expectTrue (PeaksAt<float> (-3_dB))
        .process();

    processAudioWith (HardClip<float>())
        .withInputSignal (SineWave<float>())
        .withValue (HardClip<float>::thresholdDb, -10_dB)
        .expectTrue (PeaksAt<float> (-10_dB))
        .process();
}
