#pragma once

#include <algorithm>  // min()
#include <cassert>
#include <cmath>
#include <functional>
#include <iomanip>
#include <memory>
#include <sstream>
#include <vector>

#include "hart_audio_buffer.hpp"
#include "dsp/hart_dsp_all.hpp"
#include "hart_expectation_failure_messages.hpp"
#include "matchers/hart_matcher.hpp"
#include "matchers/hart_matcher_function.hpp"
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

/// @brief Determines whether to reset the Signal in a given context
/// @ingroup TestRunner
enum class ResetSignal
{
    no,  ///< The signal will continue from whatever state it was in
    yes  ///< The signal's state will be reset
};

/// @brief A DSP host used for building and running tests inside a test case
/// @ingroup TestRunner
template <typename SampleType>
class AudioTestBuilder
{
public:
    /// @brief Moves the DSP instance into the host
    /// @details DSP instance will be moved into this host, and then returned by @ref process(), so you can re-use it.
    /// You can only pass a DSP by moving it, since some of the custom DSP wrappers can be non-copyable.
    /// If you do want to copy a DSP instance here, use its DSPBase::copy() method explicitly.
    /// @param dsp Your DSP instance
    template <typename DSPType>
    AudioTestBuilder (DSPType&& dsp,
        typename std::enable_if<
            ! std::is_lvalue_reference<DSPType&&>::value &&
            std::is_base_of<DSPBase<SampleType>, typename std::decay<DSPType>::type>::value
        >::type* = 0)
    : m_processor (std::forward<DSPType> (dsp).move())
    {
    }

    /// @brief Transfers the DSP smart pointer into the host
    /// @details Use this if your DSP does not support copying or moving. It will be owned by this host,
    /// and then returned by @ref process(), so you can re-use it.
    /// @param dsp A smart pointer to your DSP instance
    AudioTestBuilder (std::unique_ptr<DSPBase<SampleType>> dsp)
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
    /// @param durationSeconds of the signal in seconds. You can use time-related literails from @ref Units.
    AudioTestBuilder& withDuration (double durationSeconds)
    {
        if (durationSeconds < 0)
            HART_THROW_OR_RETURN(hart::ValueError, "Signal duration should be a non-negative value in seconds", *this);

        m_testDurationSeconds = durationSeconds;
        return *this;
    }

    /// @brief Adds a warm‑up period before the main test.
    /// @details The signal will be processed for this time, but no matchers will be invoked.
    /// This can be useful if your DSP uses parameter smoothers internally, that need to settle
    /// before performing the test, or has some sort of attack envelope stage, like a compressor,
    /// that you want to skip. This time will be added up with a regular test render run, i.e.
    /// `processAudioWith (...).withDuration (100_ms).withWarmUp (10_ms)` will result in
    /// 10 + 100 = 110 ms of total rendered audio.
    /// @note Calling `saveOutputTo()` (both for wav files and `AudioBuffer`s) and `savePlotTo()`
    /// will always output the entire rendered piece of audio, including this warm-up stage.
    /// @param warmUpDurationSeconds Duration of the warm‑up in seconds
    /// @param resetSignalAfterWarmUp Whether to restart the input signal generator after the warm‑up stage,
    /// see @ref hart::ResetSignal
    AudioTestBuilder& withWarmUp (double warmUpDurationSeconds, ResetSignal resetSignalAfterWarmUp = ResetSignal::no)
    {
        if (warmUpDurationSeconds < 0)
            HART_THROW_OR_RETURN (hart::ValueError, "Warm-up should be a non-negative value in seconds", *this);

        m_warmUpDurationSeconds = warmUpDurationSeconds;
        m_resetSignalAfterWarmUp = resetSignalAfterWarmUp == ResetSignal::yes;
        return *this;
    }

    /// @brief Sets the input signal for the test by copying it
    /// @param signal Input signal, see @ref Signals
    /// @param resetSignalBeforeProcessing Set to `ResetSignal::yes` if you want the runner to call
    /// Signal::reset() before rendering audio, or to `ResetSignal::no` to keep its pre-existing state
    AudioTestBuilder& withInputSignal (const SignalBase<SampleType>& signal, ResetSignal resetSignalBeforeProcessing = ResetSignal::no)
    {
        m_inputSignal = std::move (signal.copy());
        m_resetSignalBeforeProcessing = resetSignalBeforeProcessing == ResetSignal::yes;
        return *this;
    }

    /// @brief Sets the input signal for the test by moving it
    /// @param signal Input signal, see @ref Signals
    /// @param resetSignalBeforeProcessing Set to `ResetSignal::yes` if you want the runner to call
    /// Signal::reset() before rendering audio, or to `ResetSignal::no` to keep its pre-existing state
    AudioTestBuilder& withInputSignal (SignalBase<SampleType>&& signal, ResetSignal resetSignalBeforeProcessing = ResetSignal::no)
    {
        m_inputSignal = std::move (signal.move());
        m_resetSignalBeforeProcessing = resetSignalBeforeProcessing == ResetSignal::yes;
        return *this;
    }

    /// @brief Sets the input signal for the test by transfering its smart pointer
    /// @note The ownership of the smart pointer will be transferred to this class
    /// @param signal Input signal, see @ref Signals
    /// @param resetSignalBeforeProcessing Set to `ResetSignal::yes` if you want the runner to call
    /// Signal::reset() before rendering audio, or to `ResetSignal::no` to keep its pre-existing state
    AudioTestBuilder& withInputSignal (std::unique_ptr<SignalBase<SampleType>> signal, ResetSignal resetSignalBeforeProcessing = ResetSignal::no)
    {
        m_inputSignal = std::move (signal);
        m_resetSignalBeforeProcessing = resetSignalBeforeProcessing == ResetSignal::yes;
        return *this;
    }

    /// @brief Sets the input signal using a function-based signal definition
    /// @param signalFunction Function that generates the signal buffer. It will moved to a Signal object. 
    /// @param label Human-readable label for the signal to use in the test error output
    /// @param loop Determines whether the generated buffer should loop
    /// @details
    /// This overload constructs a @ref SignalFunction internally, allowing inline
    /// definition of signals without explicitly creating a Signal object, for slightly
    /// less verbose syntax.
    ///
    /// The function must have the signature:
    /// `void (AudioBuffer<SampleType>&)`
    ///
    /// This method echoes the ctor of the hart::SignalFunction class, so see its
    /// documentaion for more detailed description.
    ///
    /// @note To re-use the signal made with your function, you can use `saveInputSignalTo()`.
    /// @see SignalFunction
    AudioTestBuilder& withInputSignal (
        std::function<void (AudioBuffer<SampleType>&)> signalFunction,
        const std::string& label = {},
        Loop loop = Loop::yes)
    {
        m_inputSignal = hart::make_unique<SignalFunction<SampleType>>(
            std::move (signalFunction),
            label,
            loop
        );

        m_resetSignalBeforeProcessing = false;  // It gets constructed from scratch here anyway
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

    /// @brief Adds an "expect" check using a Matcher object
    /// @param matcher Matcher to perform the check, see @ref Matchers
    template<typename MatcherType>
    AudioTestBuilder& expectTrue (MatcherType&& matcher)
    {
        addCheck (std::forward<MatcherType> (matcher), SignalAssertionLevel::expect, true);
        return *this;
    }

    /// @brief Adds a reversed "expect" check using a Matcher object
    /// @param matcher Matcher to perform the check, see @ref Matchers
    template<typename MatcherType>
    AudioTestBuilder& expectFalse (MatcherType&& matcher)
    {
        addCheck (std::forward<MatcherType> (matcher), SignalAssertionLevel::expect, false);
        return *this;
    }

    /// @brief Adds an "assert" check using a Matcher object
    /// @param matcher Matcher to perform the check, see @ref Matchers
    template<typename MatcherType>
    AudioTestBuilder& assertTrue (MatcherType&& matcher)
    {
        addCheck (std::forward<MatcherType> (matcher), SignalAssertionLevel::assert, true);
        return *this;
    }

    /// @brief Adds a reversed "assert" check using a Matcher object
    /// @param matcher Matcher to perform the check, see @ref Matchers
    template<typename MatcherType>
    AudioTestBuilder& assertFalse (MatcherType&& matcher)
    {
        addCheck (std::forward<MatcherType> (matcher), SignalAssertionLevel::assert, false);
        return *this;
    }

    // TODO: Add expect/assert overloads for smart pointers as well

    /// @brief Adds an "expect" check using a function matcher
    /// @details Intended for simple inline expressions. For anything more than
    /// that, consider making a custom hart::Matcher subclass and use it instead.
    /// @see MatcherFunction
    /// @param matcherFunction Function with signature:
    /// @code
    /// bool(const AudioBuffer<SampleType>& output)
    /// @endcode
    /// @param label Optional label used in failure reports
    AudioTestBuilder& expectTrue (std::function<bool (const AudioBuffer<SampleType>&)> matcherFunction, const std::string& label = {})
    {
        return expectTrue (MatcherFunction<SampleType> (std::move (matcherFunction), label));
    }

    /// @brief Adds an "expect" check using a function matcher
    /// @details Intended for simple inline expressions. For anything more than
    /// that, consider making a custom hart::Matcher subclass and use it instead.
    /// @see MatcherFunction
    /// @param matcherFunction Function with signature:
    /// @code
    /// bool(const AudioBuffer<SampleType>& input,
    ///      const AudioBuffer<SampleType>& output)
    /// @endcode
    /// @param label Optional label used in failure reports
    /// @note If your matcher function only cares about the output, and not the input,
    /// just use the overload that takes `bool(const AudioBuffer<SampleType>& output)`.
    AudioTestBuilder& expectTrue (std::function<bool (const AudioBuffer<SampleType>&, const AudioBuffer<SampleType>&)> matcherFunction, const std::string& label = {})
    {
        return expectTrue (MatcherFunction<SampleType> (std::move (matcherFunction), label));
    }

    /// @brief Adds a reversed "expect" check using a function matcher
    /// @details Intended for simple inline expressions. For anything more than
    /// that, consider making a custom hart::Matcher subclass and use it instead.
    /// @see MatcherFunction
    /// @param matcherFunction Function with signature:
    /// @code
    /// bool(const AudioBuffer<SampleType>& output)
    /// @endcode
    /// @param label Optional label used in failure reports
    AudioTestBuilder& expectFalse (std::function<bool (const AudioBuffer<SampleType>&)> matcherFunction, const std::string& label = {})
    {
        return expectFalse (MatcherFunction<SampleType> (std::move (matcherFunction), label));
    }

    /// @brief Adds a reversed "expect" check using a function matcher
    /// @details Intended for simple inline expressions. For anything more than
    /// that, consider making a custom hart::Matcher subclass and use it instead.
    /// @see MatcherFunction
    /// @param matcherFunction Function with signature:
    /// @code
    /// bool(const AudioBuffer<SampleType>& input,
    ///      const AudioBuffer<SampleType>& output)
    /// @endcode
    /// @param label Optional label used in failure reports
    /// @note If your matcher function only cares about the output, and not the input,
    /// just use the overload that takes `bool(const AudioBuffer<SampleType>& output)`.
    AudioTestBuilder& expectFalse (std::function<bool (const AudioBuffer<SampleType>&, const AudioBuffer<SampleType>&)> matcherFunction, const std::string& label = {})
    {
        return expectFalse (MatcherFunction<SampleType> (std::move (matcherFunction), label));
    }

    /// @brief Adds an "assert" check using a function matcher
    /// @details Intended for simple inline expressions. For anything more than
    /// that, consider making a custom hart::Matcher subclass and use it instead.
    /// @see MatcherFunction
    /// @param matcherFunction Function with signature:
    /// @code
    /// bool(const AudioBuffer<SampleType>& output)
    /// @endcode
    /// @param label Optional label used in failure reports
    AudioTestBuilder& assertTrue (std::function<bool (const AudioBuffer<SampleType>&)> matcherFunction, const std::string& label = {})
    {
        return assertTrue (MatcherFunction<SampleType> (std::move (matcherFunction), label));
    }

    /// @brief Adds an "assert" check using a function matcher
    /// @details Intended for simple inline expressions. For anything more than
    /// that, consider making a custom hart::Matcher subclass and use it instead.
    /// @see MatcherFunction
    /// @param matcherFunction Function with signature:
    /// @code
    /// bool(const AudioBuffer<SampleType>& input,
    ///      const AudioBuffer<SampleType>& output)
    /// @endcode
    /// @param label Optional label used in failure reports
    /// @note If your matcher function only cares about the output, and not the input,
    /// just use the overload that takes `bool(const AudioBuffer<SampleType>& output)`.
    AudioTestBuilder& assertTrue (std::function<bool (const AudioBuffer<SampleType>&, const AudioBuffer<SampleType>&)> matcherFunction, const std::string& label = {})
    {
        return assertTrue (MatcherFunction<SampleType> (std::move (matcherFunction), label));
    }

    /// @brief Adds a reversed "assert" check using a function matcher
    /// @details Intended for simple inline expressions. For anything more than
    /// that, consider making a custom hart::Matcher subclass and use it instead.
    /// @see MatcherFunction
    /// @param matcherFunction Function with signature:
    /// @code
    /// bool(const AudioBuffer<SampleType>& output)
    /// @endcode
    /// @param label Optional label used in failure reports
    AudioTestBuilder& assertFalse (std::function<bool (const AudioBuffer<SampleType>&)> matcherFunction, const std::string& label = {})
    {
        return assertFalse (MatcherFunction<SampleType> (std::move (matcherFunction), label));
    }

    /// @brief Adds a reversed "assert" check using a function matcher
    /// @details Intended for simple inline expressions. For anything more than
    /// that, consider making a custom hart::Matcher subclass and use it instead.
    /// @see MatcherFunction
    /// @param matcherFunction Function with signature:
    /// @code
    /// bool(const AudioBuffer<SampleType>& input,
    ///      const AudioBuffer<SampleType>& output)
    /// @endcode
    /// @param label Optional label used in failure reports
    /// @note If your matcher function only cares about the output, and not the input,
    /// just use the overload that takes `bool(const AudioBuffer<SampleType>& output)`.
    AudioTestBuilder& assertFalse (std::function<bool (const AudioBuffer<SampleType>&, const AudioBuffer<SampleType>&)> matcherFunction, const std::string& label = {})
    {
        return assertFalse (MatcherFunction<SampleType> (std::move (matcherFunction), label));
    }

    /// @brief Enables saving output audio to a wav file
    /// @note If you're using `withWarmUp()`, this warm-up section of audio will also be included in the output file
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

    /// @brief Enables saving output audio to a provided buffer
    /// @note If you're using `withWarmUp()`, this warm-up section of audio will also be included in the output buffer
    /// @details Tip: You can use @ref HART_STR() to construct file names using "<<" syntax.
    /// @warning The target directory has to exist!
    /// @param receivingBuffer An output buffer to receive the data. You can pass an unitialised buffer, among other things, as it will be move-assigned.
    AudioTestBuilder& saveOutputTo (AudioBuffer<SampleType>& receivingBuffer)
    {
        m_outputBufferSink = [&receivingBuffer] (AudioBuffer<SampleType>&& outputBuffer)
        {
            receivingBuffer = std::move (outputBuffer);
        };

        return *this;
    }

    /// @brief Enables saving output audio via provided callback
    /// @note If you're using `withWarmUp()`, this warm-up section of audio will also be included in the output buffer
    /// @details Tip: You can use @ref HART_STR() to construct file names using "<<" syntax.
    /// @warning The target directory has to exist!
    /// @param outputBufferSink A callable that accepts a buffer rvalue. The buffer is moved into the provided sink. The test runner takes ownership of the callable object.
    AudioTestBuilder& saveOutputTo (std::function<void (AudioBuffer<SampleType>&&)> outputBufferSink)
    {
        m_outputBufferSink = std::move (outputBufferSink);
        return *this;
    }

    /// @brief Enables saving a plot to an SVG file
    /// @details This will plot an input and output audio as a waveform
    /// @note If you're using `withWarmUp()`, this warm-up section of audio will also be included in the plot
    /// Tip: You can use @ref HART_STR() to construct file names using "<<" syntax.
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

    /// @brief Moves the input signal after the processing into the provided smart pointer
    /// @details It's useful if you want to re-use your signal, query it for something,
    /// or extract some DSP instance from its DSP chain after the test.
    /// @param receivingSignal A smart pointer that will receive the moved signal
    AudioTestBuilder& saveInputSignalTo (std::unique_ptr<SignalBase<SampleType>>& receivingSignal)
    {
        m_inputSignalSink = [&receivingSignal] (std::unique_ptr<SignalBase<SampleType>>&& usedSignal)
        {
            receivingSignal = std::move (usedSignal);
        };

        return *this;
    }

    /// @brief Moves the input signal after the processing via provided callback
    /// @details It's useful if you want to re-use your signal, query it for something,
    /// or extract some DSP instance from its DSP chain after the test.
    /// @param inputSignalSink A callable that accepts the moved signal
    AudioTestBuilder& saveInputSignalTo (std::function<void (std::unique_ptr<SignalBase<SampleType>>&&)> inputSignalSink)
    {
        m_inputSignalSink = std::move (inputSignalSink);
        return *this;
    }

    /// @brief Adds a label to the test
    /// @details Useful when you call @ref process() multiple times in one test case - the label
    /// will be put into test failure report to indicate exactly which test has failed.
    /// Tip: You can use @ref HART_STR() to construct label strings using "<<" syntax.
    /// @param testLabel Any text, to be used as a label
    AudioTestBuilder& withLabel (const std::string& testLabel)
    {
        m_testLabel = testLabel;
        return *this;
    }

    /// @brief Perfoems the test
    /// @details Call this after setting all the test parameters
    std::unique_ptr<DSPBase<SampleType>> process()
    {
        const size_t totalDurationFrames = (size_t) std::round (m_sampleRateHz * (m_testDurationSeconds + m_warmUpDurationSeconds));
        const size_t warmUpDurationFrames = (size_t) std::round (m_sampleRateHz * m_warmUpDurationSeconds);
        const size_t testDurationFrames = totalDurationFrames - warmUpDurationFrames;

        if (totalDurationFrames == 0)
            HART_THROW_OR_RETURN (hart::SizeError, "Nothing to process", std::move (m_processor));

        for (auto& check : perBlockChecks)
        {
            check.matcher->prepare (m_sampleRateHz, m_numOutputChannels, m_blockSizeFrames);
            check.shouldSkip = false;
        }

        for (auto& check : fullSignalChecks)
        {
            check.matcher->prepare (m_sampleRateHz, m_numOutputChannels, m_blockSizeFrames);
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

        if (m_resetSignalBeforeProcessing)
            m_inputSignal->resetWithDSPChain();

        m_inputSignal->prepareWithDSPChain (m_sampleRateHz, m_numInputChannels, m_blockSizeFrames);
        offsetFrames = 0;

        // TODO: Pre-allocate full buffer sizes here, as they're already known at this point
        AudioBuffer<SampleType> fullInputBuffer (m_numInputChannels, 0, m_sampleRateHz);
        AudioBuffer<SampleType> fullOutputBuffer (m_numOutputChannels, 0, m_sampleRateHz);
        bool atLeastOneCheckFailed = false;

        // Warm-up render
        while (offsetFrames < warmUpDurationFrames)
        {
            const size_t blockSizeFrames = std::min (m_blockSizeFrames, warmUpDurationFrames - offsetFrames);

            hart::AudioBuffer<SampleType> inputBlock (m_numInputChannels, blockSizeFrames, m_sampleRateHz);
            hart::AudioBuffer<SampleType> outputBlock (m_numOutputChannels, blockSizeFrames, m_sampleRateHz);
            m_inputSignal->renderNextBlockWithDSPChain (inputBlock);
            m_processor->processWithEnvelopes (inputBlock, outputBlock);

            fullInputBuffer.appendFrom (inputBlock);
            fullOutputBuffer.appendFrom (outputBlock);

            offsetFrames += blockSizeFrames;
        }

        if (m_resetSignalAfterWarmUp)
            m_inputSignal->resetWithDSPChain();

        // Main test render
        while (offsetFrames < totalDurationFrames)
        {
            // TODO: Do not continue if there are no checks, or all checks should skip and there's no input and output file to write

            const size_t blockSizeFrames = std::min (m_blockSizeFrames, totalDurationFrames - offsetFrames);

            hart::AudioBuffer<SampleType> inputBlock (m_numInputChannels, blockSizeFrames, m_sampleRateHz);
            hart::AudioBuffer<SampleType> outputBlock (m_numOutputChannels, blockSizeFrames, m_sampleRateHz);
            m_inputSignal->renderNextBlockWithDSPChain (inputBlock);
            m_processor->processWithEnvelopes (inputBlock, outputBlock);

            const bool allChecksPassed = processChecks (perBlockChecks, inputBlock, outputBlock, offsetFrames);
            atLeastOneCheckFailed |= ! allChecksPassed;
            fullInputBuffer.appendFrom (inputBlock);
            fullOutputBuffer.appendFrom (outputBlock);

            offsetFrames += blockSizeFrames;
        }

        if (testDurationFrames != 0 && ! fullSignalChecks.empty())
        {
            // Full audio buffers for full audio matchers
            // We want to skip the warm-up pieces for those
            AudioBuffer<SampleType> fullInputNoWarmUpBuffer (m_numInputChannels, testDurationFrames, m_sampleRateHz);
            AudioBuffer<SampleType> fullOutputNoWarmUpBuffer (m_numOutputChannels, testDurationFrames, m_sampleRateHz);

            for (size_t channel = 0; channel < m_numInputChannels; ++channel)
                fullInputNoWarmUpBuffer.copyFrom (channel, 0, fullInputBuffer, channel, warmUpDurationFrames, testDurationFrames);

            for (size_t channel = 0; channel < m_numOutputChannels; ++channel)
                fullOutputNoWarmUpBuffer.copyFrom (channel, 0, fullOutputBuffer, channel, warmUpDurationFrames, testDurationFrames);

            const bool allChecksPassed = processChecks (fullSignalChecks, fullInputNoWarmUpBuffer, fullOutputNoWarmUpBuffer, warmUpDurationFrames);
            atLeastOneCheckFailed |= ! allChecksPassed;
        }

        if (m_saveOutputMode == Save::always || (m_saveOutputMode == Save::whenFails && atLeastOneCheckFailed))
            WavWriter<SampleType>::writeBuffer (fullOutputBuffer, m_saveOutputPath, m_saveOutputWavFormat);
    
        if (m_savePlotMode == Save::always || (m_savePlotMode == Save::whenFails && atLeastOneCheckFailed))
            plotData (fullInputBuffer, fullOutputBuffer, m_savePlotPath);

        if (m_outputBufferSink != nullptr)
            m_outputBufferSink (std::move (fullOutputBuffer));

        if (m_inputSignalSink != nullptr)
            m_inputSignalSink (std::move (m_inputSignal));

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
        std::unique_ptr<MatcherBase<SampleType>> matcher;
        SignalAssertionLevel signalAssertionLevel;
        bool shouldSkip;
        bool shouldPass;
    };

    std::unique_ptr<DSPBase<SampleType>> m_processor;
    std::unique_ptr<SignalBase<SampleType>> m_inputSignal;
    double m_sampleRateHz = (double) 44100;
    size_t m_blockSizeFrames = 1024;
    size_t m_numInputChannels = 1;
    size_t m_numOutputChannels = 1;
    std::vector<ParamValue> paramValues;
    double m_testDurationSeconds = 0.1;
    double m_warmUpDurationSeconds = 0.0;
    bool m_resetSignalAfterWarmUp = false;
    bool m_resetSignalBeforeProcessing = false;
    size_t offsetFrames = 0;
    std::string m_testLabel = {};

    std::vector<Check> perBlockChecks;
    std::vector<Check> fullSignalChecks;

    std::string m_saveOutputPath;
    Save m_saveOutputMode = Save::never;
    WavFormat m_saveOutputWavFormat = WavFormat::pcm24;

    std::string m_savePlotPath;
    Save m_savePlotMode = Save::never;

    std::function<void (AudioBuffer<SampleType>&&)> m_outputBufferSink = nullptr;
    std::function<void (std::unique_ptr<SignalBase<SampleType>>&&)> m_inputSignalSink = nullptr;

    template<
        typename MatcherType,
        typename = typename std::enable_if<
            ! std::is_same<
                typename std::decay<MatcherType>::type,
                MatcherBase<SampleType>
                >::value
        >::type>
    void addCheck (MatcherType&& matcher, SignalAssertionLevel assertionLevel, bool shouldPass)
    {
        using Derived = typename std::decay<MatcherType>::type;
        static_assert (std::is_base_of<MatcherBase<SampleType>, Derived>::value, "matcher argument must derive from hart::Matcher");

        const bool forceFullSignal = !shouldPass;
        auto& group = (matcher.canOperatePerBlock() && !forceFullSignal)
            ? perBlockChecks
            : fullSignalChecks;

        // TODO: emplace_back()
        group.push_back ({
            std::forward<MatcherType>(matcher).move(),
            assertionLevel,
            false,
            shouldPass
        });
    }

    void addCheck (const MatcherBase<SampleType>& matcher, SignalAssertionLevel assertionLevel, bool shouldPass)
    {
        const bool forceFullSignal = ! shouldPass;
        auto& group = (matcher.canOperatePerBlock() && ! forceFullSignal)
            ? perBlockChecks
            : fullSignalChecks;

        // TODO: emplace_back()
        group.push_back({ matcher.copy(), assertionLevel, false, shouldPass });
    }

    bool processChecks (std::vector<Check>& checksGroup, const AudioBuffer<SampleType>& inputAudio, const AudioBuffer<SampleType>& outputAudio, size_t baseFrameOffset)
    {
        for (auto& check : checksGroup)
        {
            if (check.shouldSkip)
                continue;

            auto& assertionLevel = check.signalAssertionLevel;
            auto& matcher = check.matcher;

            const bool matchPassed = matcher->match (inputAudio, outputAudio);

            if (matchPassed != check.shouldPass)
            {
                check.shouldSkip = true;

                if (assertionLevel == SignalAssertionLevel::assert)
                {
                    std::stringstream stream;
                    stream << (check.shouldPass ? "assertTrue() failed" : "assertFalse() failed");

                    if (! m_testLabel.empty())
                        stream << " at \"" << m_testLabel << "\"";

                    stream << std::endl << "Condition: " << *matcher;

                    if (check.shouldPass)
                        appendFailureDetails (stream, matcher->getFailureDetails(), inputAudio, outputAudio, baseFrameOffset);

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
                        appendFailureDetails (stream, matcher->getFailureDetails(), inputAudio, outputAudio, baseFrameOffset);

                    hart::ExpectationFailureMessages::get().emplace_back (stream.str());
                }

                // TODO: FIXME: Do not throw inside of per-block loop if requested to write input or output to a wav file, throw after the loop instead
                // TODO: Stop processing if expect has failed and outputting to a file wasn't requested
                // TODO: Skip all checks if check failed, but asked to output a wav file
                return false;
            }
        }

        return true;
    }

    void appendFailureDetails (std::stringstream& stream, const MatcherFailureDetails& details, const AudioBuffer<SampleType>& inputAudio, const AudioBuffer<SampleType>& observedOutputAudio, size_t baseFrameOffset)
    {
        // TODO: Display input sample info as well

        const size_t frameOverall = baseFrameOffset + details.frame;
        const double timestampOverall = static_cast<double> (frameOverall) / m_sampleRateHz;
        const size_t warmUpDurationFrames = (size_t) std::round (m_sampleRateHz * m_warmUpDurationSeconds);
        const SampleType inputSampleValue = inputAudio[details.channel][details.frame];
        const SampleType outputSampleValue = observedOutputAudio[details.channel][details.frame];

        stream << std::endl
            << "Input signal: " << *m_inputSignal << std::endl
            << "Channel: " << details.channel << std::endl;

        if (warmUpDurationFrames == 0)
        {
            stream
                << "Frame: " << frameOverall << std::endl
                << secPrecision << "Timestamp: " << timestampOverall << " seconds";
        }
        else
        {
            const size_t framePostWarmUp = frameOverall - warmUpDurationFrames;
            const double timestampPostWarmUp = static_cast<double> (framePostWarmUp) / m_sampleRateHz;
            stream
                << "Frame (overall): " << frameOverall << std::endl
                << "Frame (post warm-up): " << framePostWarmUp << std::endl
                << secPrecision
                << "Timestamp (overall): " << timestampOverall << " seconds" << std::endl
                << "Timestamp (post warm-up): " << timestampPostWarmUp << " seconds";
        }

        stream << std::endl
            << linPrecision << "Input sample value: " << inputSampleValue
            << dbPrecision << " (" << ratioToDecibels (std::abs (inputSampleValue)) << " dB)" << std::endl
            << linPrecision << "Output sample value: " << outputSampleValue
            << dbPrecision << " (" << ratioToDecibels (std::abs (outputSampleValue)) << " dB)" << std::endl
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
    return AudioTestBuilder<SampleType> (std::unique_ptr<DSPBase<SampleType>> (dsp.release()));
}

namespace aliases_float
{
    using hart::processAudioWith;
}
namespace aliases_double
{
    using hart::processAudioWith;
}

}  // namespace hart
