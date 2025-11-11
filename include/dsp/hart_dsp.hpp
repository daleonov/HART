#pragma once

#include <ostream>
#include <string>

#include "hart_audio_buffer.hpp"

namespace hart
{

template <typename SampleType, typename ParamType>
class DSP
{
public:
    virtual ~DSP() = default;
    virtual void prepare (double sampleRateHz, size_t numInputChannels, size_t numOutputChannels, size_t maxBlockSizeFrames) = 0;
    virtual void process (const AudioBuffer<SampleType>& inputs, AudioBuffer<SampleType>& outputs) = 0;
    virtual void reset() = 0;
    virtual void setValue (int id, ParamType value) = 0;
    virtual bool supportsChannelLayout (size_t numInputChannels, size_t numOutputChannels) = 0;
    virtual void print (std::ostream& stream) const = 0;
};

template <typename SampleType, typename ParamType>
inline std::ostream& operator<< (std::ostream& stream, const DSP<SampleType, ParamType>& dsp) {
    dsp.print (stream);
    return stream;
}

}  // namespace hart
