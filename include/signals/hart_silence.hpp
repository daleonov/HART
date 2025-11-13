#pragma once

#include <memory>
#include <string>

#include "signals/hart_signal.hpp"

namespace hart
{

template<typename SampleType>
class Silence:
    public Signal<SampleType>
{
public:
    void prepare (double /* sampleRateHz */, size_t numOutputChannels, size_t /* maxBlockSizeFrames */) override
    {
        setNumChannels (numOutputChannels);
    }

    void renderNextBlock (AudioBuffer<SampleType>& output) override
    {
        if (output.getNumChannels() != getNumChannels())
            HART_THROW_OR_RETURN_VOID (ChannelLayoutError, std::string ("Signal was configured for a different channel number") + describe());

        for (size_t channel = 0; channel < this->m_numChannels; ++channel)
            for (size_t frame = 0; frame < output.getNumFrames(); ++frame)
                output[channel][frame] = (SampleType) 0;
    }

    void reset() override {}

    std::unique_ptr<Signal<SampleType>> copy() const override
    {
        return std::make_unique<Silence<SampleType>> (*this);
    }

    std::string describe() const override
    {
        return "Silence";
    }

};

}  // namespace hart
