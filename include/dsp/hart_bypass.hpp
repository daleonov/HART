#pragma once

#include "dsp/hart_dsp.hpp"
#include "hart_utils.hpp"  // HART_DEFINE_GENERIC_REPRESENT()

namespace hart
{

/// @brief Bypasses the audio
/// @details Just copies the inout data to the output, as in "true bypass".
/// Useful for cases where you just want to render the signal to the output buffer as-is.
/// Selecting specific channels, like `.atChannels()` will result in only the selected
/// channels being bypassed (copied), while the rest of them will be untouched in the
/// output buffer. The data in those un-selected channels might end up being un-initialised.
/// It probably doesn't make sense to use this specific "effect" on a subset of channels,
/// but it's still supported, just to be in line with existing HART API.
/// @tparam SampleType Type of audio sample data, typically `float` or `double`.
template <typename SampleType>
class Bypass :
    public DSP<SampleType, Bypass<SampleType>>
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
            if (channelsToProcess[channel] == false)
                continue;

            const SampleType* inputData = input[channel];
            SampleType* outputData = output[channel];

            for (size_t frame = 0; frame < numFrames; ++frame)
                outputData[frame] = inputData[frame];
        }
    }

    void setValue (int /* paramId */, double /* value */)
    {
        // Doesn't support any settable parameters
        hassertfalse;
    }

    bool supportsChannelLayout (size_t numInputChannels, size_t numOutputChannels) const
    {
        return numInputChannels == numOutputChannels;
    }

    HART_DEFINE_GENERIC_REPRESENT (Bypass);
    HART_DSP_COPYABLE (Bypass);
    HART_DSP_MOVABLE (Bypass);
};

HART_DSP_DECLARE_ALIASES_FOR (Bypass);

}  // namespace hart
