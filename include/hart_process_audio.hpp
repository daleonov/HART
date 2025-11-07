#pragma once

#include <memory>
#include <vector>

#include "hart_tested_audio_processor.hpp"
#include "hart_signal.hpp"

namespace hart {

template <typename SampleType, typename ParamType>
class AudioTestBuilder
{
public:
    AudioTestBuilder (TestedAudioProcessor<SampleType, ParamType>& processor):
        m_processor (processor)
    {
    }

    AudioTestBuilder& withSampleRate (ParamType sampleRateHz)
    {
        m_sampleRateHz = sampleRateHz;
        return *this;
    }

    AudioTestBuilder& withBlockSize (size_t blockSizeFrames)
    {
        m_blockSizeFrames = blockSizeFrames;
        return *this;
    }

    AudioTestBuilder& withValue (const std::string& id, ParamType value)
    {
        paramValues.emplace_back (ParamValue { id, value });
        return *this;
    }

    AudioTestBuilder& withInputSignal (const Signal<SampleType>& signal)
    {
        m_inputSignal = std::move (signal.copy());
        return *this;
    }

    AudioTestBuilder& withInputSignal(Signal<SampleType>&& signal) {
        m_inputSignal = std::make_unique<Signal<SampleType>> (std::move (signal));
        return *this;
    }

    void process()
    {
    }

private:
    struct ParamValue
    {
        std::string id;
        ParamType value;
    };

    TestedAudioProcessor<SampleType, ParamType>& m_processor;
    std::unique_ptr<Signal<SampleType>> m_inputSignal;
    ParamType m_sampleRateHz = (ParamType) 44100;
    size_t m_blockSizeFrames = 1024;
    std::vector<ParamValue> paramValues;
};

template <typename SampleType, typename ParamType>
AudioTestBuilder<SampleType, ParamType> processAudioWith (TestedAudioProcessor<SampleType, ParamType>& processor)
{
    return { processor };
}

}  // namespace hart
