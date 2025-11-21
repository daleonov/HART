#pragma once

#include <algorithm>  // min()
#include <cassert>
#include <cmath>
#include <iomanip>
#include <memory>
#include <sstream>
#include <vector>

#include "hart_audio_buffer.hpp"
#include "dsp/hart_dsp_all.hpp"
#include "hart_expectation_failure_messages.hpp"
#include "matchers/hart_matcher.hpp"
#include "hart_plot.hpp"
#include "hart_precision.hpp"
#include "hart_wavwriter.hpp"
#include "signals/hart_signals_all.hpp"
#include "hart_utils.hpp"  // make_unique()

namespace hart {

/// @defgroup TestRunner Test Runner
/// @brief Runs the tests

/// @brief Determines when to save a file
/// @ingroup TestRunner
enum class Save
{
    always,  ///< File will be saved always, after the test is performed
    whenFails,  ///< File will be saved only when the test has failed
    never  ///< File will not be saved
};

/// @brief A DSP host used for building and running tests inside a test case
/// @ingroup TestRunner
template <typename SampleType>
class AudioTestBuilder
{
public:
    /// @brief Copies the DSP instance into the host
    /// @details DSP instance will be moved into this host, and then returned by @ref process(), so you can re-use it.
    /// @param dsp Your DSP instance.
    template <typename DSPType>
    AudioTestBuilder (DSPType&& dsp,
        typename std::enable_if<
            std::is_lvalue_reference<DSPType&&>::value &&
            std::is_base_of<DSP<SampleType>, typename std::decay<DSPType>::type>::value
        >::type* = 0)
    : m_processor (dsp.copy())
    {
    }

    /// @brief Moves the DSP instance into the host
    /// @details DSP instance will be moved into this host, and then returned by @ref process(), so you can re-use it.
    /// @param dsp Your DSP instance
    template <typename DSPType>
    AudioTestBuilder (DSPType&& dsp,
        typename std::enable_if<
            ! std::is_lvalue_reference<DSPType&&>::value &&
            std::is_base_of<DSP<SampleType>, typename std::decay<DSPType>::type>::value
        >::type* = 0)
    : m_processor (std::forward<DSPType> (dsp).move())
    {
    }

    /// @brief Transfers the DSP smart pointer into the host
    /// @details Use this if your DSP does not support copying or moving. It will be owned by this host,
    /// and then returned by @ref process(), so you can re-use it.
    /// @param dsp A smart pointer to your DSP instance
    AudioTestBuilder (std::unique_ptr<DSP<SampleType>> dsp)
    : m_processor (std::move (dsp))
    {
    }

    /// @brief Sets the sample rate for the test
    /// @details All the signals, effects and sub hosts are guaranteed to be initialized to this sample rate
    /// @param sampleRateHz Sample rate in Hz. You can use frequency-related literails from @ref Units.
    AudioTestBuilder& withSampleRate (double sampleRateHz)
    {
        if (sampleRateHz <= 0)
            HART_THROW_OR_RETURN (hart::ValueError, "Sample rate should be a positive value in Hz", *this);

        if (! m_processor->supportsSampleRate (sampleRateHz))
            HART_THROW_OR_RETURN (hart::SampleRateError, "Sample rate is not supported by the tested DSP", *this);

        m_sampleRateHz = sampleRateHz;
        return *this;
    }

    /// @brief Sets the block size for the test
    /// @param blockSizeFrames Block size in frames (samples)
    AudioTestBuilder& withBlockSize (size_t blockSizeFrames)
    {
        if (blockSizeFrames == 0)
            HART_THROW_OR_RETURN (hart::SizeError, "Illegal block size - should be a positive value in frames (samples)", *this);

        m_blockSizeFrames = blockSizeFrames;
        return *this;
    }

    /// @brief Sets the initial param value for the tested DSP
    /// @details It will call @ref DSP::setValue() for DSP under test
    /// @param id Parameter ID (see @ref DSP::setValue())
    /// @param value Value that needs to be set
    AudioTestBuilder& withValue (int id, double value)
    {
        // TODO: Handle cases when processor already has an envelope for this id
        paramValues.emplace_back (ParamValue { id, value });
        return *this;
    }

    /// @brief Sets the total duration of the input signal to be processed
    /// @param Duration of the signal in seconds. You can use time-related literails from @ref Units.
    AudioTestBuilder& withDuration (double durationSeconds)
    {
        if (durationSeconds < 0)
            HART_THROW_OR_RETURN(hart::ValueError, "Signal duration should be a non-negative value in Hz", *this);

        m_durationSeconds = durationSeconds;
        return *this;
    }

    /// @brief Sets the input signal for the test
    /// @param signal Input signal, see @ref Signals
    AudioTestBuilder& withInputSignal (const Signal<SampleType>& signal)
    {
        // TODO: Implement moving/transfering a signal instance as well
        m_inputSignal = std::move (signal.copy());
        return *this;
    }

    /// @brief Sets arbitrary number of input channels
    /// @details For common mono and stereo cases, you may use dedicated methods like @ref inStereo() or
    /// @ref withMonoInput() instead of this one for better readability.
    /// @param numInputChannels Number of input channels
    AudioTestBuilder& withInputChannels (size_t numInputChannels)
    {   
        if (numInputChannels == 0)
            HART_THROW_OR_RETURN (SizeError, "There should be at least one (mono) audio channel", *this);

        if (numInputChannels > 128)
            HART_THROW_OR_RETURN (SizeError, "The number of channels is unexpectedly large... Do people really use so many channels?", *this);

        m_numInputChannels = numInputChannels;
        return *this;
    }

    /// @brief Sets arbitrary number of output channels
    /// @details For common mono and stereo cases, you may use dedicated methods like @ref inMono() or
    /// @ref withStereoOutput() instead of this one for better readability.
    /// @param numOutputChannels Number of output channels
    AudioTestBuilder& withOutputChannels (size_t numOutputChannels)
    {   
        if (numOutputChannels == 0)
            HART_THROW_OR_RETURN(SizeError, "There should be at least one (mono) audio channel", *this);

        if (numOutputChannels > 128)
            HART_THROW_OR_RETURN(SizeError, "The number of channels is unexpectedly large... Do people really use so many channels?", *this);

        m_numOutputChannels = numOutputChannels;
        return *this;
    }

    /// @brief Sets number of input channels to two
    AudioTestBuilder& withStereoInput()
    {
        return this->withInputChannels (2);
    }

    /// @brief Sets number of output channels to two
    AudioTestBuilder& withStereoOutput()
    {
        return this->withOutputChannels (2);
    }

    /// @brief Sets number of input channels to one
    AudioTestBuilder& withMonoInput()
    {
        return this->withInputChannels (1);
    }

    /// @brief Sets number of output channels to one
    AudioTestBuilder& withMonoOutput()
    {
        return this->withOutputChannels (1);
    }

    /// @brief Sets number of input and output channels to one
    AudioTestBuilder& inMono()
    {
        return this->withMonoInput().withMonoOutput();
    }

    /// @brief Sets number of input and output channels to two
    AudioTestBuilder& inStereo()
    {
        return this->withStereoInput().withStereoOutput();
    }

    /// @brief Adds an "expect" check
    /// @param matcher Matcher to perform the check, see @ref Matchers
    AudioTestBuilder& expectTrue (const Matcher<SampleType>& matcher)
    {
        addCheck (matcher, SignalAssertionLevel::expect, true);
        return *this;
    }

    /// @brief Adds an "expect" check
    /// @param matcher Matcher to perform the check, see @ref Matchers
    template <typename MatcherType>
    AudioTestBuilder& expectTrue (MatcherType&& matcher)
    {
        using DecayedType = typename std::decay<MatcherType>::type;
        static_assert (
            std::is_base_of<Matcher<SampleType>, DecayedType>::value,
            "MatcherType must be a hart::Matcher subclass"
            );

        addCheck (std::forward<MatcherType>(matcher), SignalAssertionLevel::expect, true);
        return *this;
    }

    /// @brief Adds a reversed "expect" check
    /// @param matcher Matcher to perform the check, see @ref Matchers
    AudioTestBuilder& expectFalse (const Matcher<SampleType>& matcher)
    {
        addCheck (matcher, SignalAssertionLevel::expect, false);
        return *this;
    }

    /// @brief Adds a reversed "expect" check
    /// @param matcher Matcher to perform the check, see @ref Matchers
    template <typename MatcherType>
    AudioTestBuilder& expectFalse (MatcherType&& matcher)
    {
        using DecayedType = typename std::decay<MatcherType>::type;
        static_assert (
            std::is_base_of<Matcher<SampleType>, DecayedType>::value,
            "MatcherType must be a hart::Matcher subclass"
            );
        addCheck (std::forward<MatcherType>(matcher), SignalAssertionLevel::expect, false);
        return *this;
    }

    /// @brief Adds an "assert" check
    /// @param matcher Matcher to perform the check, see @ref Matchers
    AudioTestBuilder& assertTrue (const Matcher<SampleType>& matcher)
    {
        addCheck (matcher, SignalAssertionLevel::assert, true);
        return *this;
    }

    /// @brief Adds an "assert" check
    /// @param matcher Matcher to perform the check, see @ref Matchers
    template <typename MatcherType>
    AudioTestBuilder& assertTrue (MatcherType&& matcher)
    {
        using DecayedType = typename std::decay<MatcherType>::type;
        static_assert (
            std::is_base_of<Matcher<SampleType>, DecayedType>::value,
            "MatcherType must be a hart::Matcher subclass"
            );
        addCheck (std::forward<MatcherType>(matcher), SignalAssertionLevel::assert, true);
        return *this;
    }

    /// @brief Adds a reversed "assert" check
    /// @param matcher Matcher to perform the check, see @ref Matchers
    AudioTestBuilder& assertFalse (const Matcher<SampleType>& matcher)
    {
        addCheck (matcher, SignalAssertionLevel::assert, false);
        return *this;
    }

    /// @brief Adds a reversed "assert" check
    /// @param matcher Matcher to perform the check, see @ref Matchers
    template <typename MatcherType>
    AudioTestBuilder& assertFalse (MatcherType&& matcher)
    {
        using DecayedType = typename std::decay<MatcherType>::type;
        static_assert (
            std::is_base_of<Matcher<SampleType>, DecayedType>::value,
            "MatcherType must be a hart::Matcher subclass"
            );
        addCheck (std::forward<MatcherType>(matcher), SignalAssertionLevel::assert, false);
        return *this;
    }

    /// @brief Enables saving output audio to a wav file
    /// @param path File path - relative or absolute. If relative path is set, it will be appended to the provided `--data-root-path` CLI argument.
    /// @param mode When to save, see @ref hart::Save
    /// @param wavFormat Format of the wav file, see hart::WavFormat for supported options
    /// @see HART_REQUIRES_DATA_PATH_ARG
    AudioTestBuilder& saveOutputTo (const std::string& path, Save mode = Save::always, WavFormat wavFormat = WavFormat::pcm24)
    {
        if (path.empty())
            return *this;

        m_saveOutputPath = toAbsolutePath (path);
        m_saveOutputMode = mode;
        m_saveOutputWavFormat = wavFormat;
        return *this;
    }

    /// @brief Enables saving a plot to an SVG file
    /// @details This will plot an input and output audio as a waveform
    /// @param path File path - relative or absolute. If relative path is set, it will be appended to the provided `--data-root-path` CLI argument.
    /// @param mode When to save, see @ref hart::Save
    /// @see HART_REQUIRES_DATA_PATH_ARG
    AudioTestBuilder& savePlotTo (const std::string& path, Save mode = Save::always)
    {
        if (path.empty())
            return *this;

        m_savePlotPath = toAbsolutePath (path);
        m_savePlotMode = mode;
        return *this;
    }

    /// @brief Adds a label to the test
    /// @details Useful when you call @ref process() multiple times in one test case - the label
    /// will be put into test failure report to indicate exactly which test has failed.
    /// @param testLabel Any text, to be used as a label
    AudioTestBuilder& withLabel (const std::string& testLabel)
    {
        m_testLabel = testLabel;
        return *this;
    }

    /// @brief Perfoems the test
    /// @details Call this after setting all the test parameters
    std::unique_ptr<DSP<SampleType>> process()
    {
        m_durationFrames = (size_t) std::round (m_sampleRateHz * m_durationSeconds);

        if (m_durationFrames == 0)
            HART_THROW_OR_RETURN (hart::SizeError, "Nothing to process", std::move (m_processor));

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
            HART_THROW_OR_RETURN (hart::StateError, "No input signal - call withInputSignal() first!", std::move (m_processor));

        m_inputSignal->resetWithDSPChain();
        m_inputSignal->prepareWithDSPChain (m_sampleRateHz, m_numInputChannels, m_blockSizeFrames);
        offsetFrames = 0;

        AudioBuffer<SampleType> fullInputBuffer (m_numInputChannels);
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
            fullInputBuffer.appendFrom (inputBlock);
            fullOutputBuffer.appendFrom (outputBlock);

            offsetFrames += blockSizeFrames;
        }

        const bool allChecksPassed = processChecks (fullSignalChecks, fullOutputBuffer);
        atLeastOneCheckFailed |= ! allChecksPassed;

        if (m_saveOutputMode == Save::always || (m_saveOutputMode == Save::whenFails && atLeastOneCheckFailed))
            WavWriter<SampleType>::writeBuffer (fullOutputBuffer, m_saveOutputPath, m_sampleRateHz, m_saveOutputWavFormat);
    
        if (m_savePlotMode == Save::always || (m_savePlotMode == Save::whenFails && atLeastOneCheckFailed))
            plotData (fullInputBuffer, fullOutputBuffer, m_sampleRateHz, m_savePlotPath);

        return std::move (m_processor);
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
    size_t offsetFrames = 0;
    std::string m_testLabel = {};

    std::vector<Check> perBlockChecks;
    std::vector<Check> fullSignalChecks;

    std::string m_saveOutputPath;
    Save m_saveOutputMode = Save::never;
    WavFormat m_saveOutputWavFormat = WavFormat::pcm24;

    std::string m_savePlotPath;
    Save m_savePlotMode = Save::never;

    void addCheck (const Matcher<SampleType>& matcher, SignalAssertionLevel signalAssertionLevel, bool shouldPass)
    {
        const bool forceFullSignal = ! shouldPass;  // No per-block checks for inverted matchers
        auto& checksGroup =
            (matcher.canOperatePerBlock() && ! forceFullSignal)
                ? perBlockChecks
                : fullSignalChecks;
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
        using DecayedType = typename std::decay<MatcherType>::type;
        static_assert (
            std::is_base_of<Matcher<SampleType>, DecayedType>::value,
            "MatcherType must be a hart::Matcher subclass"
            );
        const bool forceFullSignal = ! shouldPass;  // No per-block checks for inverted matchers
        auto& checksGroup =
            (matcher.canOperatePerBlock() && ! forceFullSignal)
                ? perBlockChecks
                : fullSignalChecks;
        checksGroup.emplace_back (AudioTestBuilder::Check {
            hart::make_unique<DecayedType> (std::forward<MatcherType> (matcher)),
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
                // TODO: Add optional label for each test

                if (assertionLevel == SignalAssertionLevel::assert)
                {
                    std::stringstream stream;
                    stream << (check.shouldPass ? "assertTrue() failed" : "assertFalse() failed");

                    if (! m_testLabel.empty())
                        stream << " at \"" << m_testLabel << "\"";

                    stream << std::endl << "Condition: " << *matcher;

                    if (check.shouldPass)
                        appendFailureDetails (stream, matcher->getFailureDetails(), outputBlock);

                    throw hart::TestAssertException (std::string (stream.str()));
                }
                else
                {
                    std::stringstream stream;
                    stream << (check.shouldPass ? "expectTrue() failed" : "expectFalse() failed");

                    if (!m_testLabel.empty())
                        stream << " at \"" << m_testLabel << "\"";

                    stream << std::endl << "Condition: " << * matcher;

                    if (check.shouldPass)
                        appendFailureDetails (stream, matcher->getFailureDetails(), outputBlock);

                    hart::ExpectationFailureMessages::get().emplace_back (stream.str());
                }

                // TODO: FIXME: Do not throw indife of per-block loop if requested to write input or output to a wav file, throw after the loop instead
                // TODO: Stop processing if expect has failed and outputting to a file wasn't requested
                // TODO: Skip all checks if check failed, but asked to output a wav file
                return false;
            }
        }

        return true;
    }

    void appendFailureDetails (std::stringstream& stream, const MatcherFailureDetails& details, AudioBuffer<SampleType>& observedAudioBlock)
    {
        const double timestampSeconds = static_cast<double> (offsetFrames + details.frame) / m_sampleRateHz;
        const SampleType sampleValue = observedAudioBlock[details.channel][details.frame];

        stream << std::endl
            << "Channel: " << details.channel << std::endl
            << "Frame: " << details.frame << std::endl
            << secPrecision << "Timestamp: " << timestampSeconds << " seconds" << std::endl
            << linPrecision << "Sample value: " << sampleValue
            << dbPrecision << " (" << ratioToDecibels (std::abs (sampleValue)) << " dB)" << std::endl
            << details.description;
    }
};

/// @brief Call this to start building your test
/// @param dsp Instance of your DSP effect
/// @return @ref AudioTestBuilder instance - you can chain a bunch of test parameters with it.
/// @ingroup TestRunner
/// @relates AudioTestBuilder
template <typename DSPType>
AudioTestBuilder<typename std::decay<DSPType>::type::SampleTypePublicAlias> processAudioWith (DSPType&& dsp)
{
    return AudioTestBuilder<typename std::decay<DSPType>::type::SampleTypePublicAlias> (std::forward<DSPType>(dsp));
}

/// @brief Call this to start building your test
/// @details Call this for DSP objects that do not support moving or copying
/// @param dsp Instance of your DSP effect wrapped in a smart pointer
/// @return @ref AudioTestBuilder instance - you can chain a bunch of test parameters with it.
/// @ingroup TestRunner
/// @relates AudioTestBuilder
template <typename DSPType>
AudioTestBuilder<typename DSPType::SampleTypePublicAlias> processAudioWith (std::unique_ptr<DSPType>&& dsp)
{
    using SampleType = typename DSPType::SampleTypePublicAlias;
    return AudioTestBuilder<SampleType> (std::unique_ptr<DSP<SampleType>> (dsp.release()));
}

}  // namespace hart
