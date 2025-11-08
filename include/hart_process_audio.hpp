#pragma once

#include <algorithm>  // min()
#include <cassert>
#include <cmath>
#include <memory>
#include <vector>

#include "hart.hpp"
#include "hart_audio_buffer.hpp"
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

    AudioTestBuilder& withSampleRate (double sampleRateHz)
    {
        if (sampleRateHz <= 0)
            HART_THROW ("Sample rate should be a positive value in Hz");

        m_sampleRateHz = sampleRateHz;
        return *this;
    }

    AudioTestBuilder& withBlockSize (size_t blockSizeFrames)
    {
        if (blockSizeFrames == 0)
            HART_THROW ("Illegal block size - should be a positive value in frames (samples)");

        m_blockSizeFrames = blockSizeFrames;
        return *this;
    }

    AudioTestBuilder& withValue (const std::string& id, ParamType value)
    {
        paramValues.emplace_back (ParamValue { id, value });
        return *this;
    }

    AudioTestBuilder& withDuration (double durationSeconds)
    {
        if (durationSeconds < 0)
            HART_THROW ("Signal duration should be a non-negative value in Hz");

        m_durationSeconds = durationSeconds;
        return *this;
    }

    AudioTestBuilder& withInputSignal (const Signal<SampleType>& signal)
    {
        m_inputSignal = std::move (signal.copy());
        return *this;
    }

    template <typename Derived>
    AudioTestBuilder& withInputSignal (Derived&& signal)
    {
        m_inputSignal = std::make_unique<Derived> (std::forward<Derived> (signal));
        return *this;
    }

    AudioTestBuilder& withChannels (size_t numChannels)
    {   
        if (numChannels == 0)
            HART_THROW ("There should be at least one (mono) audio channel");

        if (numChannels > 128)
            HART_THROW ("The number of channels is unexpectedly large... Do people really use so many channels?");

        m_numChannels = numChannels;
        return *this;
    }

    AudioTestBuilder& inMono()
    {
        return this->withChannels (1);
    }

    AudioTestBuilder& inStereo()
    {
        return this->withChannels (2);
    }

    void process()
    {
        m_durationFrames = (size_t) std::round (m_sampleRateHz * m_durationSeconds);

        if (m_durationFrames == 0)
            HART_THROW ("Nothing to process");

        // TODO: Add support for different number of input and output channels
        m_processor.reset();
        m_processor.prepare (m_sampleRateHz, m_numChannels, m_blockSizeFrames);

        for (const ParamValue& paramValue : paramValues)
        {
            // TODO: Add true/false return to indicate if setting the parameter was successful
            m_processor.setValue (paramValue.id, paramValue.value);
        }

        if (m_inputSignal == nullptr)
            HART_THROW ("No input signal - call withInputSignal() first!");

        m_inputSignal->reset();
        size_t offsetFrames = 0;

        while (offsetFrames < m_durationFrames)
        {
            const size_t blockSizeFrames = std::min (m_blockSizeFrames, m_durationFrames - offsetFrames);

            hart::AudioBuffer<SampleType> inputBlock (m_numChannels, m_durationFrames);
            hart::AudioBuffer<SampleType> outputBlock (m_numChannels, m_durationFrames);
            m_inputSignal->renderNextBlock (inputBlock.getArrayOfWritePointers(), m_durationFrames);

            // TODO: Process audio with m_processor
            // TODO: Run all the requested assertions

            offsetFrames += blockSizeFrames;
        }
    }

private:
    struct ParamValue
    {
        std::string id;
        ParamType value;
    };

    TestedAudioProcessor<SampleType, ParamType>& m_processor;
    std::unique_ptr<Signal<SampleType>> m_inputSignal;
    double m_sampleRateHz = (ParamType) 44100;
    size_t m_blockSizeFrames = 1024;
    size_t m_numChannels = 1;
    std::vector<ParamValue> paramValues;
    double m_durationSeconds = 0.0;
    size_t m_durationFrames = 0;
};

template <typename SampleType, typename ParamType>
AudioTestBuilder<SampleType, ParamType> processAudioWith (TestedAudioProcessor<SampleType, ParamType>& processor)
{
    return { processor };
}

}  // namespace hart
