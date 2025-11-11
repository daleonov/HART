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
        .expectTrue (hart::PeaksAt<float> (-3_dB))
        .process();

    hart::processAudioWith (hart::Gain<float>())
        .withInputSignal (hart::SineWave<float>())
        .withSampleRate (44100.0)
        .withBlockSize (1024)
        .withDuration (0.1)
        .inMono()
        .withValue (hart::Gain<float>::gainDb, -20_dB)
        .expectTrue (hart::PeaksAt<float> (-20_dB))
        .process();

    hart::processAudioWith (hart::Gain<float>())
        .withInputSignal (hart::SineWave<float>())
        .withSampleRate (44100.0)
        .withBlockSize (1024)
        .withDuration (0.1)
        .inMono()
        .withValue (hart::Gain<float>::gainDb, +6_dB)
        .expectTrue (hart::PeaksAt<float> (+6_dB))
        .process();
}

HART_TEST ("Gain - Channel Layouts")
{
    hart::processAudioWith (hart::Gain<float>())
        .withInputSignal (hart::SineWave<float>())
        .withSampleRate (44100.0)
        .withBlockSize (1024)
        .withDuration (0.1)
        .inMono()
        .withValue (hart::Gain<float>::gainDb, -3_dB)
        .expectTrue (hart::PeaksAt<float> (-3_dB))
        .process();

    hart::processAudioWith (hart::Gain<float>())
        .withInputSignal (hart::SineWave<float>())
        .withSampleRate (44100.0)
        .withBlockSize (1024)
        .withDuration (0.1)
        .inStereo()
        .withValue (hart::Gain<float>::gainDb, -3_dB)
        .expectTrue (hart::PeaksAt<float> (-3_dB))
        .process();

    hart::processAudioWith (hart::Gain<float>())
        .withInputSignal (hart::SineWave<float>())
        .withSampleRate (44100.0)
        .withBlockSize (1024)
        .withDuration (0.1)
        .withMonoInput()
        .withStereoOutput()
        .withValue (hart::Gain<float>::gainDb, -3_dB)
        .expectTrue (hart::PeaksAt<float> (-3_dB))
        .process();

    hart::processAudioWith (hart::Gain<float>())
        .withInputSignal (hart::SineWave<float>())
        .withSampleRate (44100.0)
        .withBlockSize (1024)
        .withDuration (0.1)
        .withMonoInput()
        .withOutputChannels (5)
        .withValue (hart::Gain<float>::gainDb, -3_dB)
        .expectTrue (hart::PeaksAt<float> (-3_dB))
        .process();

}