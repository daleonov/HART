#include <cmath>  // abs(), isnan()

#include "hart.hpp"

HART_DECLARE_ALIASES_FOR_FLOAT;
using AudioBuffer = hart::AudioBuffer<float>;
using hart::floatsEqual;

HART_TEST ("Metrics - Channel Correlation")
{
    using hart::channelCorrelation;
    using hart::Channel;

    processAudioWith (GainDb (0_dB))
        .withLabel ("Identical channels")
        .withInputSignal (SineSweep())
        .inStereo()
        .expectTrue ([] (const AudioBuffer& output) { return channelCorrelation (output) > 0.999; }, "channelCorrelation() - Left vs Right")
        .process();

    processAudioWith (GainDb (0_dB))
        .withLabel ("No correlation")
        .withInputSignal (WhiteNoise())
        .inStereo()
        .expectTrue ([] (const AudioBuffer& output) { return std::abs (channelCorrelation (output)) < 0.1; }, "channelCorrelation() - Left vs Right")
        .process();

    processAudioWith (GainLinear (-1.0).atChannel (Channel::left))
        .withLabel ("Channels out of phase")
        .withInputSignal (SineSweep())
        .inStereo()
        .expectTrue ([] (const AudioBuffer& output) { return channelCorrelation (output) < -0.999; }, "channelCorrelation() - Left vs Right").process();

    processAudioWith (Mute().atChannel (Channel::left))
        .withLabel ("Muted channel")
        .withInputSignal (SineSweep())
        .inStereo()
        .expectTrue ([] (const AudioBuffer& output) { return std::isnan (channelCorrelation (output)); }, "channelCorrelation() with default channels")
        .process();

    auto multiChannelSignal = SineSweep()
        >> GainLinear (-1.0).atChannel (1)
        >> GainDb (-30_dB).atChannel (2) >> AdditiveNoise (-3_dB).atChannel (2);

    processAudioWith (GainDb (0_dB))
        .withLabel ("Multi-channel")
        .withInputChannels (4)
        .withOutputChannels (4)
        .withInputSignal (std::move (multiChannelSignal))
        .expectTrue ([] (const AudioBuffer& output) { return floatsEqual (channelCorrelation (output, 2, 1), (channelCorrelation (output, 1, 2))); }, "channelCorrelation() - Channel order does not matter - 1 vs 2")
        .expectTrue ([] (const AudioBuffer& output) { return floatsEqual (channelCorrelation (output, 1, 0), (channelCorrelation (output, 0, 1))); }, "channelCorrelation() - Channel order does not matter - 0 vs 1")
        .expectTrue ([] (const AudioBuffer& output) { return channelCorrelation (output, 0, 1) < -0.999; }, "channelCorrelation() Channels 0 vs 1")
        .expectTrue ([] (const AudioBuffer& output) { return std::abs (channelCorrelation (output, 0, 2)) < 0.5; }, "channelCorrelation() - Channels 0 vs 2")
        .expectTrue ([] (const AudioBuffer& output) { return std::abs (channelCorrelation (output, 2, 1)) < 0.5; }, "channelCorrelation() - Channels 2 vs 1")
        .expectTrue ([] (const AudioBuffer& output) { return channelCorrelation (output, 0, 3) > 0.999; }, "channelCorrelation() - Channels 0 vs 3")
        .process();
}

HART_TEST ("Crest factor")
{
    using AudioBuffer = hart::AudioBuffer<float>;
    using Channel = hart::Channel;
    using hart::floatsEqual;
    using hart::crestFactorLinear;
    using hart::crestFactorDb;
    using std::sqrt;

    processAudioWith (GainDb (0_dB))
        .withLabel ("Crest factor of a sine wave")
        .withInputSignal (SineWave())
        .inMono()
        .expectTrue ([] (const AudioBuffer& output) { return floatsEqual<double> (crestFactorLinear (output), sqrt (2.0), 1e-3); }, "Linear crest factor is around sqrt (2)")
        .expectTrue ([] (const AudioBuffer& output) { return floatsEqual<double> (crestFactorLinear (output, 0), sqrt (2.0), 1e-3); }, "Linear crest factor is around sqrt (2)")
        .expectTrue ([] (const AudioBuffer& output) { return floatsEqual<double> (crestFactorLinear (output, Channel::left), sqrt (2.0), 1e-3); }, "Linear crest factor is around sqrt (2)")
        .expectTrue ([] (const AudioBuffer& output) { return floatsEqual<double> (crestFactorDb (output), 3.01_dB, 0.01); }, "Crest factor is around 3.01 dB")
        .expectTrue ([] (const AudioBuffer& output) { return floatsEqual<double> (crestFactorDb (output, 0), 3.01_dB, 0.01); }, "Crest factor is around 3.01 dB")
        .expectTrue ([] (const AudioBuffer& output) { return floatsEqual<double> (crestFactorDb (output, Channel::left), 3.01_dB, 0.01); }, "Crest factor is around 3.01 dB")
        .process();

    hart::DSPFunction<float> halfWaveRectify (
        [] (float x) { return x < 0.0f ? 0.0f : x; },
        "Half Wave Rectifier"
        );

    processAudioWith (GainDb (0_dB))
        .withLabel ("Crest factor of a half-rectified sine wave")
        .withInputSignal (SineWave() >> std::move (halfWaveRectify))
        .inMono()
        .expectTrue ([] (const AudioBuffer& output) { return floatsEqual<double> (crestFactorLinear (output), 2.0, 1e-3); }, "Linear crest factor is around 2")
        .expectTrue ([] (const AudioBuffer& output) { return floatsEqual<double> (crestFactorDb (output), 6.02_dB, 0.01); }, "Crest factor is around 6.02 dB")
        .process();

    processAudioWith (GainDb (0_dB))
        .withLabel ("Crest factor of an impulse")
        .withInputSignal (Impulse())
        .inMono()
        .expectTrue ([] (const AudioBuffer& output) { return crestFactorLinear (output) > 20.0; }, "Linear crest factor is more than 20")
        .expectTrue ([] (const AudioBuffer& output) { return crestFactorDb (output) > 10_dB; }, "Crest factor is around 10 dB")
        .process();

    const auto sharpTransientEnvelope = SegmentedEnvelope (0_dB)
        .hold (1_ms)
        .rampTo (-60_dB, 5_ms);

    processAudioWith (GainDb (0_dB))
        .withLabel ("Crest factor of a poky sine wave")
        .withInputSignal (SineWave() >> GainDb().withEnvelope (GainDb::gainDb, sharpTransientEnvelope))
        .inMono()
        .expectTrue ([] (const AudioBuffer& output) { return crestFactorLinear (output) > 4.0; }, "Linear crest factor is more than 4")
        .expectTrue ([] (const AudioBuffer& output) { return crestFactorDb (output) > 12_dB; }, "Crest factor is more than 12 dB")
        .process();

    for (double signalLevelDb : { -60_dB, -3_dB, -0.5_dB, +1_dB, +12_dB })
    {
        processAudioWith (GainDb (0_dB))
            .withLabel (HART_STR ("Gain doesn't matter, input level: " << hart::dbPrecision << signalLevelDb << " dB"))
            .withInputSignal (SineWave() >> GainDb (signalLevelDb))
            .inMono()
            .expectTrue ([] (const AudioBuffer& output) { return floatsEqual<double> (crestFactorLinear (output), sqrt (2.0), 1e-3); }, "Linear crest factor is around sqrt (2)")
            .expectTrue ([] (const AudioBuffer& output) { return floatsEqual<double> (crestFactorDb (output, 0), 3.01_dB, 0.01); }, "Crest factor is around 3.01 dB")
            .process();
    }

    processAudioWith (GainDb (0_dB))
        .withLabel ("Multi-channel")
        .withInputSignal (SineWave() >> GainDb().withEnvelope (GainDb::gainDb, sharpTransientEnvelope).atChannels ({1, 3}))
        .withInputChannels (5)
        .withOutputChannels (5)
        .expectTrue ([] (const AudioBuffer& output) { return floatsEqual<double> (crestFactorLinear (hart::first(), output, {0, 2, 4}), sqrt (2.0), 1e-3); }, "Linear crest factor on steady channels is around sqrt (2)")
        .expectTrue ([] (const AudioBuffer& output) { return crestFactorLinear (hart::allFloatsEqualToEachOther(), output, {0, 2, 4}) == true; }, "Linear crest factor on steady channels are the same")
        .expectTrue ([] (const AudioBuffer& output) { return floatsEqual<double> (crestFactorDb (hart::first(), output, {0, 2, 4}), 3.01_dB, 0.01); }, "Crest factor on steady channels is around 3.01 dB")
        .expectTrue ([] (const AudioBuffer& output) { return crestFactorDb (hart::allFloatsEqualToEachOther(), output, {0, 2, 4}) == true; }, "Crest factors in dB on steady channels are the same")
        .expectTrue ([] (const AudioBuffer& output) { return crestFactorLinear (hart::first(), output, {1, 3}) > 4.0; }, "Linear crest factor on poky channels is more than 4")
        .expectTrue ([] (const AudioBuffer& output) { return crestFactorLinear (hart::allFloatsEqualToEachOther(), output, {1, 3}) == true ; }, "Linear crest factors on poky channels are the same")
        .expectTrue ([] (const AudioBuffer& output) { return crestFactorDb (hart::first(), output, {1, 3}) > 12_dB; }, "Crest factor in dB on poky channels is over 12 dB")
        .expectTrue ([] (const AudioBuffer& output) { return crestFactorDb (hart::allFloatsEqualToEachOther(), output, {1, 3}) == true; }, "Crest factors in dB on poky channels are the same")
        .process();
}
