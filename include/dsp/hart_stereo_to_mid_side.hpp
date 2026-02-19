#pragma once

#include "hart_dsp.hpp"
#include "hart_utils.hpp"  // Channel, MidSideChannel

namespace hart
{

/// @brief Converts regular stereo signal into mid-side signal
/// @details Mid = L + R, Side = L - R
/// @note Obviously, this DSP only supports stereo signals
/// @see hart::MidSideChannel
/// @ingroup DSP
template <typename SampleType>
class StereoToMidSide:
    public hart::DSP<SampleType, StereoToMidSide<SampleType>>
{
public:
    void prepare (double /* sampleRateHz */, size_t numInputChannels, size_t /* numOutputChannels */, size_t /* maxBlockSizeFrames */) override
    {
        if (numInputChannels != 2)
            HART_THROW_OR_RETURN_VOID (ChannelLayoutError, "StereoToMidSide only supports stereo configuration!");
    }

    void process (const AudioBuffer<SampleType>& input, AudioBuffer<SampleType>& output, const EnvelopeBuffers& /* envelopeBuffers */, ChannelFlags /* channelsToProcess */) override
    {
        hassert (output.getNumFrames() == input.getNumFrames());
        hassert (input.getNumChannels() == 2);
        hassert (input.getNumChannels() == output.getNumChannels());

        for (size_t frame = 0; frame < input.getNumFrames(); ++frame)
        {
            output[MidSideChannel::mid][frame] = input[Channel::left][frame] + input[Channel::right][frame];
            output[MidSideChannel::side][frame] = input[Channel::left][frame] - input[Channel::right][frame];
        }
    }

    virtual bool supportsChannelLayout (size_t numInputChannels, size_t numOutputChannels) const override
    {
        return numInputChannels == 2 && numOutputChannels == 2;
    }

    void setValue (int /* paramId */, double /* value */) override {}
    void reset() override {}
    HART_DEFINE_GENERIC_REPRESENT (StereoToMidSide)
};

HART_DSP_DECLARE_ALIASES_FOR (StereoToMidSide)

}  // namespace hart
