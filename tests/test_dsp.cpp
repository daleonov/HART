#include "hart.hpp"

HART_DECLARE_ALIASES_FOR_FLOAT;

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

HART_TEST ("Mute")
{
    processAudioWith (Mute())
        .withLabel ("Mute everything")
        .withInputSignal (SineWave())
        .withInputChannels (5)
        .withOutputChannels (5)
        .expectTrue (EqualsTo (Silence()))
        .process();

    processAudioWith (Mute({}))
        .withLabel ("Mute nothing")
        .withInputSignal (SineWave())
        .withInputChannels (5)
        .withOutputChannels (5)
        .expectTrue (EqualsTo (SineWave()))
        .process();

    processAudioWith (Mute ({Mute::Channel::left}))
        .withLabel ("Mute left channel")
        .withInputSignal (SineWave())
        .inStereo()
        .expectFalse (EqualsTo (Silence()))
        .expectFalse (EqualsTo (SineWave()))
        .expectFalse (EqualsTo (SineWave() >> Mute ({Mute::Channel::right})))
        .expectTrue (PeaksAt (0_dB))
        .process();

    processAudioWith (Mute ({Mute::Channel::right}))
        .withLabel ("Mute right channel")
        .withInputSignal (SineWave())
        .inStereo()
        .expectFalse (EqualsTo (Silence()))
        .expectFalse (EqualsTo (SineWave()))
        .expectFalse (EqualsTo (SineWave() >> Mute ({Mute::Channel::left})))
        .expectTrue (PeaksAt (0_dB))
        .process();

    processAudioWith (Mute (~std::bitset<64>{}.set (0).set (2)))
        .withLabel ("Mute everything except channels 0 and 2")
        .withInputSignal (SineWave())
        .withInputChannels (5)
        .withOutputChannels (5)
        .expectFalse (EqualsTo (Silence()))
        .expectTrue (PeaksAt (0_dB))
        .process();
}
