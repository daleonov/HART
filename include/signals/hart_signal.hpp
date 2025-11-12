#pragma once

#include <cmath>  // sin()
#include <cstdint>
#include <memory>
#include <random>
#include <string>

#include "hart_audio_buffer.hpp"

namespace hart {

template<typename SampleType>
class Signal
{
public:
    virtual ~Signal() = default;

    virtual void setNumChannels (size_t numChannels)
    {
        m_numChannels = numChannels;
    }

    virtual int getNumChannels() const
    {
        return m_numChannels;
    }

    virtual void prepare (double sampleRateHz, size_t numOutputChannels, size_t maxBlockSizeFrames) = 0;
    virtual void renderNextBlock (AudioBuffer<SampleType>& output) = 0;
    virtual void reset() = 0;
    virtual std::unique_ptr<Signal<SampleType>> copy() const = 0;
    virtual std::string describe() const = 0;
    
    using m_SampleType = SampleType;

protected:
    size_t m_numChannels = 1;
};

}  // namespace hart
