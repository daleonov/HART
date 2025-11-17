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
        .withLabel ("Gain as mute button")
        .withInputSignal (SineWave())
        .withValue (GainDb::gainDb, -oo_dB)
        .expectTrue (EqualsTo (Silence()))
        .process();

    processAudioWith (GainDb())
        .withLabel ("Gain as true byppass effect")
        .withInputSignal (SineWave())
        .withValue (GainDb::gainDb, 0_dB)
        .expectTrue (EqualsTo (SineWave()))
        .process();

    processAudioWith (GainDb())
        .withLabel ("Attenuation")
        .withInputSignal (SineWave())
        .withValue (GainDb::gainDb, -3_dB)
        .expectTrue (PeaksAt (-3_dB))
        .process();

    processAudioWith (GainDb())
        .withLabel ("Strong attenuation")
        .withInputSignal (SineWave())
        .withValue (GainDb::gainDb, -20_dB)
        .expectTrue (PeaksAt (-20_dB))
        .process();

    processAudioWith (GainDb())
        .withLabel ("Boost")
        .withInputSignal (SineWave())
        .withValue (GainDb::gainDb, +6_dB)
        .expectTrue (PeaksAt (+6_dB))
        .process();
}

HART_TEST ("GainDb - Channel Layouts")
{
    processAudioWith (GainDb())
        .withLabel ("Mono")
        .withInputSignal (SineWave())
        .inMono()
        .withValue (GainDb::gainDb, -3_dB)
        .expectTrue (PeaksAt(-3_dB))
        .process();

    processAudioWith (GainDb())
        .withLabel ("Stereo")
        .withInputSignal (SineWave())
        .inStereo()
        .withValue (GainDb::gainDb, -3_dB)
        .expectTrue (PeaksAt (-3_dB))
        .process();

    processAudioWith (GainDb())
        .withLabel ("Mono in, stereo out")
        .withInputSignal (SineWave())
        .withMonoInput()
        .withStereoOutput()
        .withValue (GainDb::gainDb, -3_dB)
        .expectTrue (PeaksAt (-3_dB))
        .process();

    processAudioWith (GainDb())
        .withLabel ("Mono in, many channels out")
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
        .withLabel ("HardClip as mute button")
        .withInputSignal (SineWave())
        .withValue (HardClip::thresholdDb, -oo_dB)
        .expectTrue (EqualsTo (Silence()))
        .process();

    processAudioWith (HardClip())
        .withLabel ("HardClip as safe range clamp")
        .withInputSignal (SineWave())
        .withValue (HardClip::thresholdDb, 0_dB)
        .expectTrue (EqualsTo (SineWave()))
        .process();

    processAudioWith (HardClip())
        .withLabel ("Moderate clipping")
        .withInputSignal (SineWave())
        .withValue (HardClip::thresholdDb, -3_dB)
        .expectTrue (PeaksAt (-3_dB))
        .process();

    processAudioWith (HardClip())
        .withLabel ("More extreme clipping")
        .withInputSignal (SineWave())
        .withValue (HardClip::thresholdDb, -10_dB)
        .expectTrue (PeaksAt (-10_dB))
        .process();
}
