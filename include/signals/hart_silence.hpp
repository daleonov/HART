#pragma once

#include <memory>
#include <string>

#include "signals/hart_signal.hpp"

namespace hart
{

/// @brief Produces silence (zeros)
/// @ingroup Signals
template<typename SampleType>
class Silence:
    public Signal<SampleType>
{
public:
    bool supportsNumChannels (size_t numChannels) const override { return true; };

    void prepare (double /* sampleRateHz */, size_t /* numOutputChannels */, size_t /* maxBlockSizeFrames */) override
    {
    }

    void renderNextBlock (AudioBuffer<SampleType>& output) override
    {
        for (size_t channel = 0; channel < output.getNumChannels(); ++channel)
            for (size_t frame = 0; frame < output.getNumFrames(); ++frame)
                output[channel][frame] = (SampleType) 0;
    }

    void reset() override {}

    std::string describe() const override
    {
        return "Silence";
    }

    HART_SIGNAL_DEFINE_COPY_AND_MOVE (Silence);
};

}  // namespace hart
