#pragma once
#include <string>

namespace hart
{

template <typename SampleType, typename ParamType>
class TestedAudioProcessor
{
public:
    virtual ~TestedAudioProcessor() = default;
    virtual void prepare (double sampleRateHz, size_t numChannels, size_t maxBlockSizeFrames) = 0;
    virtual void process (const SampleType** inputs, SampleType** outputs, size_t numFrames) = 0;
    virtual void setValue (const std::string& id, ParamType value) = 0;
    virtual ParamType getValue (const std::string& id) = 0;
};

}  // namespace hart
