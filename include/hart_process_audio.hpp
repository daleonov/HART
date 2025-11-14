#pragma once

#include <algorithm>  // min()
#include <cassert>
#include <cmath>
#include <memory>
#include <vector>

#include "hart.hpp"
#include "hart_audio_buffer.hpp"
#include "dsp/hart_dsp_all.hpp"
#include "hart_expectation_failure_messages.hpp"
#include "hart_matchers.hpp"
#include "hart_wavwriter.hpp"
#include "signals/hart_signals_all.hpp"

namespace hart {

enum class Save
{
    always,
    whenFails,
    never
};

template <typename SampleType>
class AudioTestBuilder
{
public:
    // TODO: Add move semantics
    AudioTestBuilder (const DSP<SampleType>& processor):
        m_processor (processor.copy())
    {
    }

    AudioTestBuilder& withSampleRate (double sampleRateHz)
    {
        if (sampleRateHz <= 0)
            HART_THROW_OR_RETURN (hart::ValueError, "Sample rate should be a positive value in Hz", *this);

        m_sampleRateHz = sampleRateHz;
        return *this;
    }

    AudioTestBuilder& withBlockSize (size_t blockSizeFrames)
    {
        if (blockSizeFrames == 0)
            HART_THROW_OR_RETURN (hart::SizeError, "Illegal block size - should be a positive value in frames (samples)", *this);

        m_blockSizeFrames = blockSizeFrames;
        return *this;
    }

    AudioTestBuilder& withValue (int id, double value)
    {
        // TODO: Handle cases when processor already has an envelope for this id
        paramValues.emplace_back (ParamValue { id, value });
        return *this;
    }

    AudioTestBuilder& withDuration (double durationSeconds)
    {
        if (durationSeconds < 0)
            HART_THROW_OR_RETURN(hart::ValueError, "Signal duration should be a non-negative value in Hz", *this);

        m_durationSeconds = durationSeconds;
        return *this;
    }

    AudioTestBuilder& withInputSignal (const Signal<SampleType>& signal)
    {
        m_inputSignal = std::move (signal.copy());
        return *this;
    }

    AudioTestBuilder& withInputChannels (size_t numInputChannels)
    {   
        if (numInputChannels == 0)
            HART_THROW_OR_RETURN (SizeError, "There should be at least one (mono) audio channel", *this);

        if (numInputChannels > 128)
            HART_THROW_OR_RETURN (SizeError, "The number of channels is unexpectedly large... Do people really use so many channels?", *this);

        m_numInputChannels = numInputChannels;
        return *this;
    }

    AudioTestBuilder& withOutputChannels (size_t numOutputChannels)
    {   
        if (numOutputChannels == 0)
            HART_THROW_OR_RETURN(SizeError, "There should be at least one (mono) audio channel", *this);

        if (numOutputChannels > 128)
            HART_THROW_OR_RETURN(SizeError, "The number of channels is unexpectedly large... Do people really use so many channels?", *this);

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
        return this->withMonoInput().withMonoOutput();
    }

    AudioTestBuilder& inStereo()
    {
        return this->withStereoInput().withStereoOutput();
    }

    // TODO: withLabel() for easier troubleshooting

    AudioTestBuilder& expectTrue (const Matcher<SampleType>& matcher)
    {
        addCheck (matcher, SignalAssertionLevel::expect, true);
        return *this;
    }

    template <typename MatcherType>
    AudioTestBuilder& expectTrue (MatcherType&& matcher)
    {
        static_assert (std::is_base_of_v<Matcher<SampleType>, std::decay_t<MatcherType>>, "MatcherType must be a hart::Matcher subclass");
        addCheck (std::forward<MatcherType>(matcher), SignalAssertionLevel::expect, true);
        return *this;
    }

    AudioTestBuilder& expectFalse (const Matcher<SampleType>& matcher)
    {
        addCheck (matcher, SignalAssertionLevel::expect, false);
        return *this;
    }

    template <typename MatcherType>
    AudioTestBuilder& expectFalse (MatcherType&& matcher)
    {
        static_assert (std::is_base_of_v<Matcher<SampleType>, std::decay_t<MatcherType>>, "MatcherType must be a hart::Matcher subclass");
        addCheck (std::forward<MatcherType>(matcher), SignalAssertionLevel::expect, false);
        return *this;
    }

    AudioTestBuilder& assertTrue (const Matcher<SampleType>& matcher)
    {
        addCheck (matcher, SignalAssertionLevel::assert, true);
        return *this;
    }

    template <typename MatcherType>
    AudioTestBuilder& assertTrue (MatcherType&& matcher)
    {
        static_assert (std::is_base_of_v<Matcher<SampleType>, std::decay_t<MatcherType>>, "MatcherType must be a hart::Matcher subclass");
        addCheck (std::forward<MatcherType>(matcher), SignalAssertionLevel::assert, true);
        return *this;
    }

    AudioTestBuilder& assertFalse (const Matcher<SampleType>& matcher)
    {
        addCheck (matcher, SignalAssertionLevel::assert, false);
        return *this;
    }

    template <typename MatcherType>
    AudioTestBuilder& assertFalse (MatcherType&& matcher)
    {
        static_assert (std::is_base_of_v<Matcher<SampleType>, std::decay_t<MatcherType>>, "MatcherType must be a hart::Matcher subclass");
        addCheck (std::forward<MatcherType>(matcher), SignalAssertionLevel::assert, false);
        return *this;
    }

    AudioTestBuilder& saveOutputTo (const std::string& path, Save mode = Save::always, WavFormat wavFormat = WavFormat::pcm24)
    {
        if (path.empty())
            return *this;

        m_saveOutputPath = toAbsolutePath (path);
        m_saveOutputMode = mode;
        m_saveOutputWavFormat = wavFormat;
        return *this;
    }

    void process()
    {
        m_durationFrames = (size_t) std::round (m_sampleRateHz * m_durationSeconds);

        if (m_durationFrames == 0)
            HART_THROW_OR_RETURN_VOID (hart::SizeError, "Nothing to process");

        for (auto& check : perBlockChecks)
        {
            check.matcher->prepare (m_sampleRateHz, m_numOutputChannels, m_blockSizeFrames);
            check.matcher->reset();
            check.shouldSkip = false;
        }

        for (auto& check : fullSignalChecks)
        {
            check.matcher->prepare (m_sampleRateHz, m_numOutputChannels, m_blockSizeFrames);
            check.matcher->reset();
            check.shouldSkip = false;
        }

        // TODO: Ckeck supportsChannelLayout() here
        m_processor->reset();
        m_processor->prepareWithEnvelopes (m_sampleRateHz, m_numInputChannels, m_numOutputChannels, m_blockSizeFrames);

        for (const ParamValue& paramValue : paramValues)
        {
            // TODO: Add true/false return to indicate if setting the parameter was successful
            m_processor->setValue (paramValue.id, paramValue.value);
        }

        if (m_inputSignal == nullptr)
            HART_THROW_OR_RETURN_VOID (hart::StateError, "No input signal - call withInputSignal() first!");

        m_inputSignal->resetWithDSPChain();
        m_inputSignal->prepareWithDSPChain (m_sampleRateHz, m_numInputChannels, m_blockSizeFrames);
        size_t offsetFrames = 0;

        AudioBuffer<SampleType> fullOutputBuffer (m_numOutputChannels);
        bool atLeastOneCheckFailed = false;

        while (offsetFrames < m_durationFrames)
        {
            // TODO: Do not continue if there are no checks, or all checks should skip and there's no input and output file to write

            const size_t blockSizeFrames = std::min (m_blockSizeFrames, m_durationFrames - offsetFrames);

            hart::AudioBuffer<SampleType> inputBlock (m_numInputChannels, blockSizeFrames);
            hart::AudioBuffer<SampleType> outputBlock (m_numOutputChannels, blockSizeFrames);
            m_inputSignal->renderNextBlockWithDSPChain (inputBlock);
            m_processor->processWithEnvelopes (inputBlock, outputBlock);

            const bool allChecksPassed = processChecks (perBlockChecks, outputBlock);
            atLeastOneCheckFailed |= ! allChecksPassed;
            fullOutputBuffer.appendFrom (outputBlock);

            offsetFrames += blockSizeFrames;
        }

        const bool allChecksPassed = processChecks (fullSignalChecks, fullOutputBuffer);
        atLeastOneCheckFailed |= ! allChecksPassed;

        if (m_saveOutputMode == Save::always || (m_saveOutputMode == Save::whenFails && atLeastOneCheckFailed))
            WavWriter<SampleType>::writeBuffer (fullOutputBuffer, m_saveOutputPath, m_sampleRateHz, m_saveOutputWavFormat);
    }

private:
    struct ParamValue
    {
        int id;
        double value;
    };

    enum class SignalAssertionLevel
    {
        expect,
        assert,
    };

    struct Check
    {
        std::unique_ptr<Matcher<SampleType>> matcher;
        SignalAssertionLevel signalAssertionLevel;
        bool shouldSkip;
        bool shouldPass;
    };

    std::unique_ptr<DSP<SampleType>> m_processor;
    std::unique_ptr<Signal<SampleType>> m_inputSignal;
    double m_sampleRateHz = (double) 44100;
    size_t m_blockSizeFrames = 1024;
    size_t m_numInputChannels = 1;
    size_t m_numOutputChannels = 1;
    std::vector<ParamValue> paramValues;
    double m_durationSeconds = 0.1;
    size_t m_durationFrames = static_cast<size_t> (m_durationSeconds * m_sampleRateHz);

    std::vector<Check> perBlockChecks;
    std::vector<Check> fullSignalChecks;

    std::string m_saveOutputPath;
    Save m_saveOutputMode = Save::never;
    WavFormat m_saveOutputWavFormat = WavFormat::pcm24;

    void addCheck (const Matcher<SampleType>& matcher, SignalAssertionLevel signalAssertionLevel, bool shouldPass)
    {
        auto& checksGroup = matcher.canOperatePerBlock() ? perBlockChecks : fullSignalChecks;
        checksGroup.emplace_back (AudioTestBuilder::Check {
            matcher.copy(),
            signalAssertionLevel,
            false,  // shouldSkip
            shouldPass
        });
    }

    template <typename MatcherType>
    void addCheck (MatcherType&& matcher, SignalAssertionLevel signalAssertionLevel, bool shouldPass)
    {
        static_assert (std::is_base_of_v<Matcher<SampleType>, std::decay_t<MatcherType>>, "MatcherType must be a hart::Matcher subclass");
        auto& checksGroup = matcher.canOperatePerBlock() ? perBlockChecks : fullSignalChecks;
        checksGroup.emplace_back (AudioTestBuilder::Check {
            std::make_unique<std::decay_t<MatcherType>> (std::forward<MatcherType> (matcher)),
            signalAssertionLevel,
            false,  // shouldSkip
            shouldPass
        });
    }

    bool processChecks (std::vector<Check>& checksGroup, AudioBuffer<SampleType>& outputBlock)
    {
        for (auto& check : checksGroup)
        {
            if (check.shouldSkip)
                continue;

            auto& assertionLevel = check.signalAssertionLevel;
            auto& matcher = check.matcher;

            const bool matchPassed = matcher->match (outputBlock);

            if (matchPassed != check.shouldPass)
            {
                check.shouldSkip = true;

                if (assertionLevel == SignalAssertionLevel::assert)
                    throw hart::TestAssertException (std::string ("Assert failed: ") + matcher->describe());
                else
                    hart::ExpectationFailureMessages::get().emplace_back (std::string ("Expect failed: ") + matcher->describe());

                // TODO: FIXME: Do not throw indife of per-block loop if requested to write input or output to a wav file, throw after the loop instead
                // TODO: Stop processing if expect has failed and outputting to a file wasn't requested
                // TODO: Skip all checks if check failed, but asked to output a wav file
                return false;
            }
        }

        return true;
    }
};

// TODO: Add move semantics
template <typename SampleType>
AudioTestBuilder<SampleType> processAudioWith (const DSP<SampleType>& processor)
{
    return { processor };
}

}  // namespace hart
