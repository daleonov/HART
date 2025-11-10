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

    void renderNextBlock (SampleType* const* outputs, size_t numFrames) override
    {
        for (size_t channel = 0; channel < this->m_numChannels; ++channel)
            for (size_t frame = 0; frame < numFrames; ++frame)
                outputs[channel][frame] = (SampleType) 0;
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
