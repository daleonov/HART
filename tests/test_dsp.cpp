#include <cmath>  // tanh()

#include "hart.hpp"

HART_DECLARE_ALIASES_FOR_FLOAT;
using AudioBuffer = hart::AudioBuffer<float>;
using hart::Channel;
using hart::MidSideChannel;
using hart::ratioToDecibels;
using std::tanh;

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
        .expectTrue (PeaksAt (-3_dB).atChannel (Channel::left))
        .expectTrue (PeaksAt (0_dB).atChannel (Channel::right))
        .process();

    processAudioWith (GainDb (-3_dB).atChannels ({0, 3, 4}))
        .withLabel ("Active on 3 out of 5 channels")
        .withInputSignal (SineWave())
        .withInputChannels (5)
        .withOutputChannels (5)
        .expectFalse (PeaksAt (-3_dB))
        .expectTrue (PeaksAt (0_dB))
        .expectTrue (PeaksAt (-3_dB).atChannels ({0, 3, 4}))
        .expectTrue (PeaksAt (0_dB).atChannels ({1, 2}))
        .expectTrue (PeaksAt (0_dB).atChannels ({0, 1, 2}))  // Channel 0 peaks at -3dB, but collectively they peak at 0dB
        .expectTrue (PeaksAt (-3_dB).atChannel (0))
        .expectTrue (PeaksAt (0_dB).atChannel (1))
        .expectTrue (PeaksAt (0_dB).atChannel (2))
        .expectTrue (PeaksAt (-3_dB).atChannel (3))
        .expectTrue (PeaksAt (-3_dB).atChannel (4))
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

HART_TEST ("GainLinear - Specific Channels")
{
    constexpr double gainLinear = 0.1;
    const double expectedPeakDb = hart::ratioToDecibels (gainLinear);

    processAudioWith (GainLinear (gainLinear).atChannel (Channel::right))
        .withLabel ("Right Only")
        .withInputSignal (SineWave())
        .inStereo()
        .expectFalse (PeaksAt (expectedPeakDb))
        .expectTrue (PeaksAt (0_dB))
        .expectTrue (PeaksAt (expectedPeakDb).atChannel (Channel::right))
        .expectTrue (PeaksAt (0_dB).atChannel (Channel::left))
        .process();

    processAudioWith (GainLinear (gainLinear).atChannels ({0, 3}))
        .withLabel ("Active on 2 out of 5 channels")
        .withInputSignal (SineWave())
        .withInputChannels (5)
        .withOutputChannels (5)
        .expectFalse (PeaksAt (expectedPeakDb))
        .expectTrue (PeaksAt (0_dB))
        .expectTrue (PeaksAt (expectedPeakDb).atChannels ({0, 3}))
        .expectTrue (PeaksAt (0_dB).atChannels ({1, 2, 4}))
        .expectTrue (PeaksAt (expectedPeakDb).atChannel (0))
        .expectTrue (PeaksAt (0_dB).atChannel (1))
        .expectTrue (PeaksAt (0_dB).atChannel (2))
        .expectTrue (PeaksAt (expectedPeakDb).atChannel (3))
        .expectTrue (PeaksAt (0_dB).atChannel (4))
        .process();
}

HART_TEST ("HardClip - Specific Channels")
{
    processAudioWith (HardClip (-3_dB).atChannel (Channel::right))
        .withLabel ("Right Only")
        .withInputSignal (SineWave())
        .inStereo()
        .expectFalse (PeaksAt (-3_dB))
        .expectTrue (PeaksAt (0_dB))
        .expectTrue (PeaksAt (-3_dB).atChannel (Channel::right))
        .expectTrue (PeaksAt (0_dB).atChannel (Channel::left))
        .process();

    processAudioWith (HardClip (-3_dB).atChannels ({0, 2, 3}))
        .withLabel ("Active on 3 out of 5 channels")
        .withInputSignal (SineWave())
        .withInputChannels (5)
        .withOutputChannels (5)
        .expectFalse (PeaksAt (-3_dB))
        .expectTrue (PeaksAt (0_dB))
        .expectTrue (PeaksAt (-3_dB).atChannels ({0, 2, 3}))
        .expectTrue (PeaksAt (0_dB).atChannels ({1, 4}))
        .expectTrue (PeaksAt (-3_dB).atChannel (0))
        .expectTrue (PeaksAt (0_dB).atChannel (1))
        .expectTrue (PeaksAt (-3_dB).atChannel (2))
        .expectTrue (PeaksAt (-3_dB).atChannel (3))
        .expectTrue (PeaksAt (0_dB).atChannel (4))
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

    processAudioWith (Mute().atChannels ({}))
        .withLabel ("Mute nothing")
        .withInputSignal (SineWave())
        .withInputChannels (5)
        .withOutputChannels (5)
        .expectTrue (EqualsTo (SineWave()))
        .process();

    processAudioWith (Mute().atChannel (Channel::left))
        .withLabel ("Mute left channel")
        .withInputSignal (SineWave())
        .inStereo()
        .expectFalse (EqualsTo (Silence()))
        .expectFalse (EqualsTo (SineWave()))
        .expectFalse (EqualsTo (SineWave() >> Mute().atChannel (Channel::right)))
        .expectTrue (PeaksAt (0_dB))
        .expectTrue (EqualsTo (Silence()).atChannel (Channel::left))
        .expectTrue (EqualsTo (SineWave()).atChannel (Channel::right))
        .process();

    processAudioWith (Mute().atChannel (Channel::right))
        .withLabel ("Mute right channel")
        .withInputSignal (SineWave())
        .inStereo()
        .expectFalse (EqualsTo (Silence()))
        .expectFalse (EqualsTo (SineWave()))
        .expectFalse (EqualsTo (SineWave() >> Mute().atChannel (Channel::left)))
        .expectTrue (PeaksAt (0_dB))
        .expectTrue (EqualsTo (SineWave()).atChannel (Channel::left))
        .expectTrue (EqualsTo (Silence()).atChannel (Channel::right))
        .process();

    processAudioWith (Mute().atAllChannelsExcept ({0, 2}))
        .withLabel ("Mute everything except channels 0 and 2")
        .withInputSignal (SineWave())
        .withInputChannels (5)
        .withOutputChannels (5)
        .expectFalse (EqualsTo (Silence()))
        .expectTrue (PeaksAt (0_dB))
        .expectTrue (EqualsTo (SineWave()).atChannels ({0, 2}))
        .expectTrue (EqualsTo (Silence()).atChannels ({1, 3, 4}))
        .process();
}

HART_TEST ("StereoToMidSide")
{
    processAudioWith (StereoToMidSide())
        .withLabel ("Side channel cancellation")
        .withInputSignal (SineWave() >> GainLinear (0.5))
        .inStereo()
        .expectTrue (EqualsTo (Silence()).atChannel (MidSideChannel::side))
        .expectTrue (EqualsTo (SineWave()).atChannel (MidSideChannel::mid))
        .process();

    processAudioWith (StereoToMidSide())
        .withLabel ("Mid channel cancellation")
        .withInputSignal (SineWave() >> GainLinear (0.5) >> GainLinear (-1.0).atChannel (Channel::right))
        .inStereo()
        .expectTrue (EqualsTo (Silence()).atChannel (MidSideChannel::mid))
        .expectTrue (EqualsTo (SineWave()).atChannel (MidSideChannel::side))
        .process();

    processAudioWith (StereoToMidSide())
        .withLabel ("No cancellation for true stereo")
        .withInputSignal (WhiteNoise())
        .inStereo()
        .expectFalse (EqualsTo (Silence()).atChannel (MidSideChannel::mid))
        .expectFalse (EqualsTo (Silence()).atChannel (MidSideChannel::side))
        .process();
}

HART_TEST ("DSPFucntion")
{
    const float expectedSamplePeakDb = ratioToDecibels (tanh (1.0f));

    auto dspSampleWise = DSPFunction (
        [] (float x) { return tanh (x); },
        "Soft Clipper A"
        );

    auto dspBlockWiseReplacing = DSPFunction (
        [] (AudioBuffer& buffer)
        {
            for (size_t channel = 0; channel < buffer.getNumChannels(); ++channel)
            {
                float* channelData = buffer[channel];

                for (size_t frame = 0; frame < buffer.getNumFrames(); ++frame)
                    channelData[frame] = tanh (channelData[frame]);
            }
        },
        "Soft Clipper B"
        );

    auto dspBlockWiseNonReplacing = DSPFunction (
        [] (const AudioBuffer& input, AudioBuffer& output)
        {
            if (input.getNumChannels() != output.getNumChannels())
                return;

            for (size_t channel = 0; channel < input.getNumChannels(); ++channel)
            {
                const float* inputChannelData = input[channel];
                float* outputChannelData = output[channel];

                for (size_t frame = 0; frame < input.getNumFrames(); ++frame)
                    outputChannelData[frame] = tanh (inputChannelData[frame]);
            }
        },
        "Soft Clipper C"
        );

    processAudioWith (std::move (dspSampleWise))
        .withLabel ("Sample-wise named DSP")
        .withInputSignal (SineWave())
        .inStereo()
        .expectTrue (PeaksAt (expectedSamplePeakDb))
        .expectTrue (PeaksAt (expectedSamplePeakDb).atChannel (Channel::left))
        .expectTrue (PeaksAt (expectedSamplePeakDb).atChannel (Channel::right))
        .process();
}
