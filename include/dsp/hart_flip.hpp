#pragma once

#include "dsp/hart_dsp.hpp"
#include "hart_utils.hpp"  // HART_DEFINE_GENERIC_REPRESENT()

namespace hart
{

/// @brief Flips the polarity
/// @details Just flips the sign of every sample at selected channels.
/// @tparam SampleType Type of audio sample data, typically `float` or `double`.
template <typename SampleType>
class Flip :
    public DSP<SampleType, Flip<SampleType>>
{
public:
    void prepare (double /* sampleRateHz */, size_t /* numInputChannels */, size_t /* numOutputChannels */, size_t /* maxBlockSizeFrames */) override
    {
    }

    void process (const AudioBuffer<SampleType>& input, AudioBuffer<SampleType>& output, const EnvelopeBuffers& /* envelopeBuffers */, ChannelFlags channelsToProcess) override
    {
        hassert (input.getNumChannels() == output.getNumChannels());
        hassert (input.getNumFrames() == output.getNumFrames());

        const size_t numChannels = input.getNumChannels();
        const size_t numFrames = input.getNumFrames();

        for (size_t channel = 0; channel < numChannels; ++channel)
        {
            const SampleType* inputData = input[channel];
            SampleType* outputData = output[channel];

            if (channelsToProcess[channel] == true)
            {
                // Flip polarity
                for (size_t frame = 0; frame < numFrames; ++frame)
                    outputData[frame] = -inputData[frame];
            }
            else
            {
                // Bypass
                for (size_t frame = 0; frame < numFrames; ++frame)
                    outputData[frame] = inputData[frame];
            }
        }
    }

    void setValue (int /* paramId */, double /* value */) override
    {
        // Doesn't support any settable parameters
        hassertfalse;
    }

    bool supportsChannelLayout (size_t numInputChannels, size_t numOutputChannels) const override
    {
        return numInputChannels == numOutputChannels;
    }

    HART_DEFINE_GENERIC_REPRESENT (Flip);
    HART_DSP_COPYABLE (Flip);
    HART_DSP_MOVABLE (Flip);
};

HART_DSP_DECLARE_ALIASES_FOR (Flip);

}  // namespace hart
