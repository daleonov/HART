#pragma once

#include <algorithm>  // fill
#include <bitset>
#include <iomanip>  // hex

#include "hart_dsp.hpp"
#include "hart_exceptions.hpp"

namespace hart
{

/// @brief Mutes selected channels in the signal
/// @details Fills all specified channels to zeros.
/// By default, mutes all the channels, unless specific channels are set.
/// @ingroup DSP
template <typename SampleType>
class Mute:
    public hart::DSP<SampleType, Mute<SampleType>>
{
public:
    void prepare (double /*sampleRateHz*/, size_t numInputChannels, size_t numOutputChannels, size_t /*maxBlockSizeFrames*/) override
    {
        if (numInputChannels != numOutputChannels)
            HART_THROW_OR_RETURN_VOID (hart::ChannelLayoutError, "Unsupported channel configuration");
    }

    void process (const AudioBuffer<SampleType>& input, AudioBuffer<SampleType>& output, const EnvelopeBuffers& /*envelopeBuffers*/, ChannelFlags channelsToProcess) override
    {
        if (input.getNumChannels() != output.getNumChannels())
            HART_THROW_OR_RETURN_VOID (hart::ChannelLayoutError, "Channel number mismatch");

        const size_t numChannels = output.getNumChannels();
        const size_t numFrames = output.getNumFrames();

        for (size_t channel = 0; channel < numChannels; ++channel)
        {
            if (channelsToProcess[channel] == true)
                output.clear (channel, 0, numFrames);
            else
                output.copyFrom (channel, 0, input, channel, 0, numFrames);
        }
    }

    void reset() override {}

    void setValue (int /*id*/, double /*value*/) override {}

    double getValue (int /*id*/) const override { return 0.0; }

    bool supportsChannelLayout (size_t numInputChannels, size_t numOutputChannels) const override
    {
        return numInputChannels == numOutputChannels;
    }

    bool supportsEnvelopeFor(int /*id*/) const override
    {
        return false;
    }

    HART_DEFINE_GENERIC_REPRESENT (Mute)
};

HART_DSP_DECLARE_ALIASES_FOR(Mute)

}  // namespace hart
