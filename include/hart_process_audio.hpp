#pragma once

#include <algorithm>  // min()
#include <cassert>
#include <cmath>
#include <memory>
#include <vector>

#include "hart.hpp"
#include "hart_audio_buffer.hpp"
#include "hart_matchers.hpp"
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

    AudioTestBuilder& withInputChannels (size_t numInputChannels)
    {   
        if (numInputChannels == 0)
            HART_THROW ("There should be at least one (mono) audio channel");

        if (numInputChannels > 128)
            HART_THROW ("The number of channels is unexpectedly large... Do people really use so many channels?");

        m_numInputChannels = numInputChannels;
        return *this;
    }

    AudioTestBuilder& withOutputChannels (size_t numOutputChannels)
    {   
        if (numOutputChannels == 0)
            HART_THROW ("There should be at least one (mono) audio channel");

        if (numOutputChannels > 128)
            HART_THROW ("The number of channels is unexpectedly large... Do people really use so many channels?");

        m_numOutputChannels = numOutputChannels;
        return *this;
    }

    AudioTestBuilder& withStereoInput()
    {
        return this->withInputChannels (2);
    }

    AudioTestBuilder& withStereoOutput()
    {
        return this->withOutputChannels (2);
    }

    AudioTestBuilder& withMonoInput()
    {
        return this->withInputChannels (1);
    }

    AudioTestBuilder& withMonoOutput()
    {
        return this->withOutputChannels (1);
    }

    AudioTestBuilder& inMono()
    {
        return this->withMonoInput().withMonoIOutput();
    }

    AudioTestBuilder& inStereo()
    {
        return this->withStereoInput().withStereoOutput();
    }

    AudioTestBuilder& expectTrue (const Matcher<SampleType>& matcher)
    {
        checks.emplace_back (SignalAssertionLevel::expect, matcher.copy());
        return *this;
    }

    template <typename MatcherType>
    AudioTestBuilder& expectTrue (MatcherType&& matcher)
    {
        static_assert (std::is_base_of_v<Matcher<SampleType>, std::decay_t<MatcherType>>, "MatcherType must be a hart::Matcher subclass");
        checks.emplace_back (SignalAssertionLevel::expect, std::make_unique<std::decay_t<MatcherType>> (std::forward<MatcherType> (matcher)));
        return *this;
    }

    void process()
    {
        m_durationFrames = (size_t) std::round (m_sampleRateHz * m_durationSeconds);

        if (m_durationFrames == 0)
            HART_THROW ("Nothing to process");

        if (checks.size() == 0)
            HART_THROW ("Nothing to check");

        for (auto& check : checks)
        {
            auto& matcher = check.second;
            matcher->prepare (m_sampleRateHz, m_numOutputChannels, m_blockSizeFrames);
            matcher->reset();
        }

        // TODO: Add support for different number of input and output channels
        m_processor.reset();
        m_processor.prepare (m_sampleRateHz, m_numInputChannels, m_numOutputChannels, m_blockSizeFrames);

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

            hart::AudioBuffer<SampleType> inputBlock (m_numInputChannels, m_durationFrames);
            hart::AudioBuffer<SampleType> outputBlock (m_numOutputChannels, m_durationFrames);
            m_inputSignal->renderNextBlock (inputBlock.getArrayOfWritePointers(), m_durationFrames);
            m_processor.process (inputBlock.getArrayOfReadPointers(), outputBlock.getArrayOfWritePointers(), blockSizeFrames);
            
            for (auto& check : checks)
            {
                auto& assertionLevel = check.first;
                auto& matcher = check.second;

                const bool matchFailed = ! matcher->match (outputBlock);

                if (matchFailed)
                {
                    if (assertionLevel == SignalAssertionLevel::assert)
                        throw hart::TestAssertException (std::string ("Assert failed: ") + matcher->describe());
                    else
                        hart::expectationFailureMessages.emplace_back (std::string ("Expect failed: ") + matcher->describe());
                }

                matcher->prepare (m_sampleRateHz, m_numOutputChannels, m_blockSizeFrames);
                matcher->reset();
            }

            offsetFrames += blockSizeFrames;
        }
    }

private:
    struct ParamValue
    {
        std::string id;
        ParamType value;
    };

    enum class SignalAssertionLevel
    {
        expect,
        assert,
    };

    TestedAudioProcessor<SampleType, ParamType>& m_processor;
    std::unique_ptr<Signal<SampleType>> m_inputSignal;
    double m_sampleRateHz = (ParamType) 44100;
    size_t m_blockSizeFrames = 1024;
    size_t m_numInputChannels = 1;
    size_t m_numOutputChannels = 1;
    std::vector<ParamValue> paramValues;
    double m_durationSeconds = 0.0;
    size_t m_durationFrames = 0;

    std::vector<std::pair<SignalAssertionLevel, std::unique_ptr<Matcher<SampleType>>>> checks;
};

template <typename SampleType, typename ParamType>
AudioTestBuilder<SampleType, ParamType> processAudioWith (TestedAudioProcessor<SampleType, ParamType>& processor)
{
    return { processor };
}

}  // namespace hart
