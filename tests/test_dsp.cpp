#include "hart.hpp"

using EqualsTo = hart::EqualsTo<float>;
using HardClip = hart::HardClip<float>;
using GainDb = hart::GainDb<float>;
using PeaksAt = hart::PeaksAt<float>;
using hart::processAudioWith;
using Silence = hart::Silence<float>;
using SineWave = hart::SineWave<float>;

HART_TEST ("GainDb - GainDb Values")
{
    processAudioWith (GainDb())
        .withInputSignal (SineWave())
        .withValue (GainDb::gainDb, -oo_dB)
        .expectTrue (EqualsTo (Silence()))
        .process();

    processAudioWith (GainDb())
        .withInputSignal (SineWave())
        .withValue (GainDb::gainDb, 0_dB)
        .expectTrue (EqualsTo (SineWave()))
        .process();

    processAudioWith (GainDb())
        .withInputSignal (SineWave())
        .withValue (GainDb::gainDb, -3_dB)
        .expectTrue (PeaksAt (-3_dB))
        .process();

    processAudioWith (GainDb())
        .withInputSignal (SineWave())
        .withValue (GainDb::gainDb, -20_dB)
        .expectTrue (PeaksAt (-20_dB))
        .process();

    processAudioWith (GainDb())
        .withInputSignal (SineWave())
        .withValue (GainDb::gainDb, +6_dB)
        .expectTrue (PeaksAt (+6_dB))
        .process();
}

HART_TEST ("GainDb - Channel Layouts")
{
    processAudioWith (GainDb())
        .withInputSignal (SineWave())
        .inMono()
        .withValue (GainDb::gainDb, -3_dB)
        .expectTrue (PeaksAt(-3_dB))
        .process();

    processAudioWith (GainDb())
        .withInputSignal (SineWave())
        .inStereo()
        .withValue (GainDb::gainDb, -3_dB)
        .expectTrue (PeaksAt (-3_dB))
        .process();

    processAudioWith (GainDb())
        .withInputSignal (SineWave())
        .withMonoInput()
        .withStereoOutput()
        .withValue (GainDb::gainDb, -3_dB)
        .expectTrue (PeaksAt (-3_dB))
        .process();

    processAudioWith (GainDb())
        .withInputSignal (SineWave())
        .withMonoInput()
        .withOutputChannels (5)
        .withValue (GainDb::gainDb, -3_dB)
        .expectTrue (PeaksAt (-3_dB))
        .process();
}

HART_TEST ("HardClip - Threshold Values")
{
    processAudioWith (HardClip())
        .withInputSignal (SineWave())
        .withValue (HardClip::thresholdDb, -oo_dB)
        .expectTrue (EqualsTo (Silence()))
        .process();

    processAudioWith (HardClip())
        .withInputSignal (SineWave())
        .withValue (HardClip::thresholdDb, 0_dB)
        .expectTrue (EqualsTo (SineWave()))
        .process();

    processAudioWith (HardClip())
        .withInputSignal (SineWave())
        .withValue (HardClip::thresholdDb, -3_dB)
        .expectTrue (PeaksAt (-3_dB))
        .process();

    processAudioWith (HardClip())
        .withInputSignal (SineWave())
        .withValue (HardClip::thresholdDb, -10_dB)
        .expectTrue (PeaksAt (-10_dB))
        .process();
}
