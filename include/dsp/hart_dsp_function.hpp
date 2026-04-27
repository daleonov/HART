#pragma once

#include <functional>

#include "hart_dsp.hpp"
#include "hart_exceptions.hpp"

namespace hart
{

/// @brief A DSP processor defined by a user-provided function
///
/// @details
/// It allows defining audio processing logic using a lambda or
/// function object, without defining a dedicated DSP subclass.
///
/// Three function styles are supported:
///
/// @par 1. Sample-wise processing
/// @code
/// SampleType (SampleType value)
/// @endcode
/// Processes each sample independently.
///
/// @par 2. Block-wise replacing (in-place) processing
/// @code
/// void (AudioBuffer<SampleType>& buffer)
/// @endcode
/// Processes the buffer in-place. The buffer is pre-filled with input data.
///
/// @par 3. Block-wise non-replacing processing
/// @code
/// void (const AudioBuffer<SampleType>& input, AudioBuffer<SampleType>& output)
/// @endcode
/// Provides separate input and output buffers.
///
/// @par Buffer invariants
/// For mutable buffers in block-wise replacing and non-replacing modes:
/// - Number of frames must not be changed
/// - Number of channels must not be changed
/// - Sample rate must not be changed
///
/// Disobeying those constraints will result in a runtime error.
///
/// @par Example
/// @code
/// auto myDsp = DSPFunction<float> (
///     [] (float x) { return x * 0.5f; },
///     "Gain"
/// );
/// @endcode
///
/// @note This class is primarily intended for simple and expressive DSP
/// definitions in tests. For more complex or stateful processors, it's
/// encouraged to implement a dedicated DSP subclass.
/// @ingroup DSP
template<typename SampleType>
class DSPFunction :
    public DSP<SampleType, DSPFunction<SampleType>>
{
public:
    /// @brief Creates a DSP instance based on a sample-wise DSP function
    /// @note If you need to be aware of sample rate value, or what channel
    /// a given sample is from, use block-wise function signature instead
    /// (see other ctors of this class for details). Or define a full-featured
    /// DSP subclass for your DSP, instead of a simple function-based one.
    /// @details
    /// This is the simplest form of a custom DSP. The function is applied
    /// independently to each sample in the input signal.
    ///
    /// @par Example
    /// @code
    /// auto myDsp = DSPFunction<float> (
    ///     [] (float x) { return x * 0.5f; },
    ///     "Gain"
    /// );
    /// @endcode
    /// @param dspFunction Function with signature:
    /// @code
    /// SampleType (SampleType value)
    /// @endcode
    /// Will be called to process each sample, expected to return a processed value.
    /// @param label Optional human-readable label to use in test error reporting.
    DSPFunction (std::function<SampleType (SampleType)> dspFunction, const std::string& label = {}):
        m_dspFunctionSampleWise (std::move (dspFunction)), m_label (label)
    {
    }

    /// @brief Creates a DSP instance based on a block-wise replacing (in-place) DSP function.
    /// @details The provided function will be called for each block of input audio.
    /// It will receive a buffer pre-filled with input audio data. The function is expected
    /// to modify the buffer directly. The provided buffer is guaranteed to have:
    /// - A number of channels equal to `max (inputChannels, outputChannels)`
    /// - Input channels populated with input data
    /// - Additional channels (if any) initialized to zero
    /// - Number of frames equal to the block size specified in the test case,
    ///   or less than that in case of partial blocks.
    /// - Correct sample rate value in its metadata (see AudioBuffer::getSampleRateHz())
    ///
    /// For the provided buffer, the function must not change:
    /// - Number of channels
    /// - Number of frames
    /// - Sample rate
    ///
    /// Disobeying those constraints will result in a runtime error.
    /// @par Example
    /// @code
    /// auto myDsp = DSPFunction<float> (
    ///     [] (auto& buffer)
    ///     {
    ///         // Process buffer in-place
    ///     },
    ///     "My DSP function"
    /// );
    /// @endcode
    /// @param dspFunction Function with signature:
    /// @code
    /// void (AudioBuffer<SampleType>& buffer)
    /// @endcode
    /// Expected to read the input samples in the buffer and replace them with processed data.
    /// @param label Optional human-readable label to use in test error reporting.
    DSPFunction (std::function<void (AudioBuffer<SampleType>&)> dspFunction, const std::string& label = {}):
        m_dspFunctionBlockWiseReplacing (std::move (dspFunction)), m_label (label)
    {
    }

    /// @brief Creates a DSP instance based on a block-wise non-replacing (separate input and output buffers) DSP function.
    /// @details
    /// This form provides full control over how output is generated from input.
    /// The output buffer is pre-allocated with the expected size.
    ///
    /// For the output buffer, the function must not change:
    /// - Number of channels
    /// - Number of frames
    /// - Sample rate
    ///
    /// Disobeying those constraints will result in a runtime error.
    ///
    /// @par Example
    /// @code
    /// auto myDsp = DSPFunction<float> (
    ///     [] (const auto& in, auto& out)
    ///     {
    ///         // Fill output using input
    ///     },
    ///     "My DSP function"
    /// );
    /// @endcode
    /// @param dspFunction Function with signature:
    /// @code
    /// void (const AudioBuffer<SampleType>& input,
    ///       AudioBuffer<SampleType>& output)
    /// @endcode
    /// Expected to fill the output buffer with processed data from the input buffer.
    /// @param label Optional human-readable label to use in test error reporting.
    DSPFunction (std::function<void (const AudioBuffer<SampleType>&, AudioBuffer<SampleType>&)> dspFunction, const std::string& label = {}):
        m_dspFunctionBlockWiseNonReplacing (std::move (dspFunction)), m_label (label)
    {
    }

    void prepare (double sampleRateHz, size_t numInputChannels, size_t numOutputChannels, size_t maxBlockSizeFrames) override
    {
        // Make sure you've passed one of the correct function signatures to the DSP ctor!
        hassert (
            m_dspFunctionBlockWiseNonReplacing != nullptr
            || m_dspFunctionBlockWiseReplacing != nullptr
            || m_dspFunctionSampleWise != nullptr);

        if (m_dspFunctionBlockWiseReplacing != nullptr)
        {
            const size_t maxChannels = std::max (numInputChannels, numOutputChannels);
            m_inputOutputBuffer = AudioBuffer<SampleType> (maxChannels, maxBlockSizeFrames, sampleRateHz);
        }
    }

    void process (const AudioBuffer<SampleType>& input, AudioBuffer<SampleType>& output, const EnvelopeBuffers& /* envelopeBuffers */, ChannelFlags /* channelsToProcess*/ ) override
    {
        if (m_dspFunctionBlockWiseNonReplacing != nullptr)
        {
            processBlockWiseNonReplacing (input, output);
            return;
        }

        if (m_dspFunctionBlockWiseReplacing != nullptr)
        {
            processBlockWiseReplacing (input, output);
            return;
        }

        if (m_dspFunctionSampleWise != nullptr)
        {
            processSampleWise (input, output);
            return;
        }

        HART_THROW_OR_RETURN_VOID (hart::NullPointerError, "DSP function is nullptr");
    }

    void represent (std::ostream& stream) const override
    {
        stream << "DSPFunction (<function>, \"" << m_label << "\")";
    }

    void reset() override {}
    void setValue (int /*paramId*/, double /*value*/) override {}
    bool supportsSampleRate (double /*sampleRateHz*/) const override { return true; }
    bool supportsEnvelopeFor (int /*paramId*/) const override { return false; }

    bool supportsChannelLayout (size_t numInputChannels, size_t numOutputChannels) const override
    {
        // There's no graceful way to handle non-matching channel numbers with sample-wise
        // function (other than perhaps mono-to-many), hence refusing to render those.
        if (m_dspFunctionSampleWise != nullptr)
            return numInputChannels == numOutputChannels;

        return true;
    }

private:
    void processSampleWise (const AudioBuffer<SampleType>& input, AudioBuffer<SampleType>& output)
    {
        // supportsChannelLayout() should reject non-matching channel numbers for sample-wise function
        hassert (input.getNumChannels() == output.getNumChannels());

        const size_t numChannels = input.getNumChannels();
        const size_t numFrames = input.getNumFrames();

        for (size_t channel = 0; channel < numChannels; ++channel)
            for (size_t i = 0; i < numFrames; ++i)
                output[channel][i] = m_dspFunctionSampleWise (input[channel][i]);
    }

    void processBlockWiseReplacing (const AudioBuffer<SampleType>& input, AudioBuffer<SampleType>& output)
    {
        const size_t numFrames = input.getNumFrames();

        if (m_inputOutputBuffer.getNumFrames() != numFrames)
            m_inputOutputBuffer.setNumFrames (numFrames);  // Assuming it's a switch to a partial block, or back from it

        for (size_t channel = 0; channel < input.getNumChannels(); ++channel)
            m_inputOutputBuffer.copyFrom (channel, 0, input, channel, 0, numFrames);

        for (size_t channel = input.getNumChannels(); channel < m_inputOutputBuffer.getNumChannels(); ++channel)
            m_inputOutputBuffer.clear (channel, 0, numFrames);

        const size_t originalNumChannels = m_inputOutputBuffer.getNumChannels();
        const size_t originalNumFrames = m_inputOutputBuffer.getNumFrames();
        const double originalSampleRateHz = m_inputOutputBuffer.getSampleRateHz();

        m_dspFunctionBlockWiseReplacing (m_inputOutputBuffer);

        if (m_inputOutputBuffer.getNumChannels() != originalNumChannels)
            HART_THROW_OR_RETURN_VOID (hart::ChannelLayoutError, "In-place DSP must not change the number of channels in the buffer");

        if (m_inputOutputBuffer.getNumFrames() != originalNumFrames)
            HART_THROW_OR_RETURN_VOID (hart::SizeError, "In-place DSP must not change the buffer length");

        if (!floatsEqual (m_inputOutputBuffer.getSampleRateHz(), originalSampleRateHz))
            HART_THROW_OR_RETURN_VOID (hart::SampleRateError, "In-place DSP must not change buffer's sample rate");

        for (size_t channel = 0; channel < output.getNumChannels(); ++channel)
            output.copyFrom (channel, 0, m_inputOutputBuffer, channel, 0, numFrames);
    }

    void processBlockWiseNonReplacing (const AudioBuffer<SampleType>& input, AudioBuffer<SampleType>& output)
    {
        const size_t originalNumChannels = output.getNumChannels();
        const size_t originalNumFrames = output.getNumFrames();
        const double originalSampleRateHz = output.getSampleRateHz();

        m_dspFunctionBlockWiseNonReplacing (input, output);

        // User's function is only allowed to modify the actual frames, and nothing else

        if (output.getNumChannels() != originalNumChannels)
            HART_THROW_OR_RETURN_VOID (hart::ChannelLayoutError, "DSP must not change number of channels in the output buffer");

        if (output.getNumFrames() != originalNumFrames)
            HART_THROW_OR_RETURN_VOID (hart::SizeError, "DSP must not change the output buffer length");

        if (! floatsEqual (output.getSampleRateHz(), originalSampleRateHz))
            HART_THROW_OR_RETURN_VOID (hart::SampleRateError, "DSP must not change sample rate of the output buffer");
    }

private:
    std::function<SampleType (SampleType)> m_dspFunctionSampleWise = nullptr;
    std::function<void (AudioBuffer<SampleType>&)> m_dspFunctionBlockWiseReplacing = nullptr;
    std::function<void (const AudioBuffer<SampleType>&, AudioBuffer<SampleType>&)> m_dspFunctionBlockWiseNonReplacing = nullptr;
    std::string m_label;
    AudioBuffer<SampleType> m_inputOutputBuffer;
};

HART_DSP_DECLARE_ALIASES_FOR (DSPFunction)

}  // namespace hart
