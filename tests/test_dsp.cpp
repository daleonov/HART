#include "hart.hpp"

HART_DECLARE_ALIASES_FOR_FLOAT;
using hart::Channel;

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
        .withLabel ("5 channels in, 5 channels out")
        .withInputSignal (SineWave())
        .withInputChannels (5)
        .withOutputChannels (5)
        .withValue (GainDb::gainDb, -3_dB)
        .expectTrue (PeaksAt (-3_dB))
        .process();
}

HART_TEST ("GainDb - Specific Channels")
{
    processAudioWith (GainDb (-3_dB).atChannel (Channel::left))
        .withLabel ("Left Only")
        .withInputSignal (SineWave())
        .inStereo()
        .expectFalse (PeaksAt (-3_dB))
        .expectTrue (PeaksAt (0_dB))
        .expectTrue (PeaksAt (-3_dB).forChannel (Channel::left))
        .expectTrue (PeaksAt (0_dB).forChannel (Channel::right))
        .process();

    processAudioWith (GainDb (-3_dB).atChannels ({0, 3, 4}))
        .withLabel ("Active on 3 out of 5 channels")
        .withInputSignal (SineWave())
        .withInputChannels (5)
        .withOutputChannels (5)
        .expectFalse (PeaksAt (-3_dB))
        .expectTrue (PeaksAt (0_dB))
        .expectTrue (PeaksAt (-3_dB).forChannels ({0, 3, 4}))
        .expectTrue (PeaksAt (0_dB).forChannels ({1, 2}))
        .expectTrue (PeaksAt (0_dB).forChannels ({0, 1, 2}))  // Channel 0 peaks at -3dB, but collectively they peak at 0dB
        .expectTrue (PeaksAt (-3_dB).forChannel (0))
        .expectTrue (PeaksAt (0_dB).forChannel (1))
        .expectTrue (PeaksAt (0_dB).forChannel (2))
        .expectTrue (PeaksAt (-3_dB).forChannel (3))
        .expectTrue (PeaksAt (-3_dB).forChannel (4))
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

    processAudioWith (Mute ({Channel::left}))
        .withLabel ("Mute left channel")
        .withInputSignal (SineWave())
        .inStereo()
        .expectFalse (EqualsTo (Silence()))
        .expectFalse (EqualsTo (SineWave()))
        .expectFalse (EqualsTo (SineWave() >> Mute ({Channel::right})))
        .expectTrue (PeaksAt (0_dB))
        .expectTrue (EqualsTo (Silence()).forChannel (Channel::left))
        .expectTrue (EqualsTo (SineWave()).forChannel (Channel::right))
        .process();

    processAudioWith (Mute ({Channel::right}))
        .withLabel ("Mute right channel")
        .withInputSignal (SineWave())
        .inStereo()
        .expectFalse (EqualsTo (Silence()))
        .expectFalse (EqualsTo (SineWave()))
        .expectFalse (EqualsTo (SineWave() >> Mute ({Channel::left})))
        .expectTrue (PeaksAt (0_dB))
        .expectTrue (EqualsTo (SineWave()).forChannel (Channel::left))
        .expectTrue (EqualsTo (Silence()).forChannel (Channel::right))
        .process();

    processAudioWith (Mute (~std::bitset<64>{}.set (0).set (2)))
        .withLabel ("Mute everything except channels 0 and 2")
        .withInputSignal (SineWave())
        .withInputChannels (5)
        .withOutputChannels (5)
        .expectFalse (EqualsTo (Silence()))
        .expectTrue (PeaksAt (0_dB))
        .expectTrue (EqualsTo (SineWave()).forChannels ({0, 2}))
        .expectTrue (EqualsTo (Silence()).forChannels ({1, 3, 4}))
        .process();
}
