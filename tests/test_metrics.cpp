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
