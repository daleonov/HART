#pragma once

#include <algorithm>
#include <functional>
#include <memory>

#include "signals/hart_signal.hpp"
#include "hart_utils.hpp"  // floatsEqual(), Loop

// TODO: Allow user buffer mismatch, if it's mono, and do multiplexing?

namespace hart
{

/// @brief Signal defined by a user-provided function
/// @ingroup Signals
///
/// @details
/// This Signal allows defining input signals using a lambda or function object,
///  instead of creating a dedicated Signal subclass.
///
/// The provided function is called during @ref prepare() and is expected to fill an
/// initially empty @ref AudioBuffer with audio data. This buffer is then used as a
/// source and streamed block-by-block during processing.
///
/// The generated buffer can either:
/// - loop continuously (SignalFunction::Loop::yes), or
/// - play once and output silence afterwards (SignalFunction::Loop::no)
///
/// `SignalFunction` can be suitable for:
/// - Procedural signal generation (e.g. waveforms, noise)
/// - Defining short repeating patterns (e.g. one-cycle signals)
/// - Creating custom test signals inline in test cases
///
/// @par Example
/// @code
// auto nyquistSignal = SignalFunction (
///     [] (AudioBuffer& buffer)
///     {
///         buffer.setNumFrames (2);
///         for (size_t channel = 0; channel < buffer.getNumChannels(); ++channel)
///         {
///             buffer[channel][0] = -1.0f;
///             buffer[channel][0] = 1.0f;
///         }
///     },
///     "Nyquist Signal",
///     SignalFunction::Loop::yes
/// );
/// @endcode
///
/// @throws hart::NullPointerError if `signalFunction` is empty
/// @throws hart::ChannelLayoutError if the function changes buffer's channel count
/// @throws hart::SampleRateError if the function changes buffer's sample rate
/// @throws hart::SizeError if the function does not allocate any frames
///
/// @ingroup Signals
template<typename SampleType>
class SignalFunction:
    public Signal<SampleType, SignalFunction<SampleType>>
{
public:
    /// @brief Constructs a signal from a user-defined function.
    /// @details
    /// The provided function is guaranteed to be called with an empty
    /// @ref AudioBuffer, i.e. AudioBuffer::getNumFrames() will return zero.
    /// The function must allocate and fill the buffer with audio data.
    /// The provided buffer is guaranteed to contain valid metadata on required
    /// number of channels (see @ref AudioBuffer::getNumChannels()) and
    /// sample rate (see @ref AudioBuffer::getSampleRateHz()).
    ///
    /// The buffer is owned internally and reused during processing.
    ///
    /// @param signalFunction A callable with signature:
    /// `void (AudioBuffer<SampleType>&)`
    ///
    /// For the provided buffer, the function must:
    /// - Allocate number of frames necessary for your Signal
    /// - Fill the buffer with valid audio data
    /// - Preserve the number of channels and sample rate
    /// Failure to meet these requirements will result in a runtime error.
    ///
    /// @param label A human-readable description of the signal, used in logs
    /// and failure reports. Can be empty.
    ///
    /// @param loop If set to Loop::yes, the generated buffer will repeat
    /// continuously. If set to Loop::no, the signal will output silence
    /// (zeros) after the buffer is exhausted.
    SignalFunction (std::function <void (AudioBuffer<SampleType>&)> signalFunction, const std::string& label = {}, Loop loop = Loop::yes) :
        m_signalFunction (std::move (signalFunction)),
        m_label (label),
        m_loop (loop)
    {
    }

    void renderNextBlock (AudioBuffer<SampleType>& output) override
    {
        // Sanity check
        hassert (m_userBuffer != nullptr);

        // User is not alowed to sabbotage with their buffer in any way
        hassert (m_userBuffer->getNumChannels() == output.getNumChannels());
        hassert (m_userBuffer->getNumFrames() != 0);
        hassert (
            m_userBuffer->hasSampleRate()
            && output.hasSampleRate()
            && floatsEqual (output.getSampleRateHz(), m_userBuffer->getSampleRateHz())
            );

        const size_t bufferFrames = m_userBuffer->getNumFrames();
        const size_t numFrames = output.getNumFrames();
        const size_t numChannels = output.getNumChannels();

        size_t readPos = m_userBufferOffsetFrames;
        size_t outputOffsetFrames = 0;

        while (outputOffsetFrames < numFrames)
        {
            const size_t framesAvailable = bufferFrames - readPos;
            const size_t framesToCopy = std::min (framesAvailable, numFrames - outputOffsetFrames);

            for (size_t channel = 0; channel < numChannels; ++channel)
            {
                std::copy (
                    (*m_userBuffer)[channel] + readPos,
                    (*m_userBuffer)[channel] + readPos + framesToCopy,
                    output[channel] + outputOffsetFrames
                );
            }

            outputOffsetFrames += framesToCopy;
            readPos += framesToCopy;

            if (readPos >= bufferFrames)
            {
                if (m_loop == Loop::yes)
                    readPos = 0;
                else
                    break;
            }
        }

        while (outputOffsetFrames < numFrames)
        {
            for (size_t channel = 0; channel < numChannels; ++channel)
                output[channel][outputOffsetFrames] = static_cast<SampleType> (0);

            ++outputOffsetFrames;
        }

        m_userBufferOffsetFrames = readPos;
    }

    void prepare (double sampleRateHz, size_t numOutputChannels, size_t /*maxBlockSizeFrames*/) override
    {
        // If internal buffer has some audio in it, we expect it to have a certain sample rate
        if (m_userBuffer != nullptr)
            hassert (m_userBuffer->hasSampleRate());

        const bool bufferDoesntNeedGenerating =
            m_userBuffer != nullptr
            && m_userBuffer->getNumFrames() > 0
            && numOutputChannels == m_userBuffer->getNumChannels()
            && floatsEqual (sampleRateHz, m_userBuffer->getSampleRateHz());

        if (bufferDoesntNeedGenerating)
            return;

        m_userBuffer = std::make_shared<AudioBuffer<SampleType>> (numOutputChannels, 0, sampleRateHz);

        if (m_signalFunction == nullptr)
            HART_THROW_OR_RETURN_VOID (hart::NullPointerError, "Signal function is a nullptr!");

        hassert (m_userBuffer->getNumFrames() == 0);  // Guaranteed to provide an empty buffer
        m_signalFunction (*m_userBuffer);

        if (m_userBuffer->getNumChannels() != numOutputChannels)
            HART_THROW_OR_RETURN_VOID (hart::ChannelLayoutError, "Your signal function shouldn't alter the number of buffer's channels");

        if (! m_userBuffer->hasSampleRate() || ! floatsEqual (sampleRateHz, m_userBuffer->getSampleRateHz()))
            HART_THROW_OR_RETURN_VOID (hart::SampleRateError, "Your signal function shouldn't alter the buffer's sample rate");

        if (m_userBuffer->getNumFrames() == 0)
            HART_THROW_OR_RETURN_VOID (hart::SizeError, "Your signal function should allocate at least one frame in the buffer");
    }

    void reset() override
    {
        m_userBufferOffsetFrames = 0;
    }

    void represent (std::ostream& stream) const override
    {
        stream << "SignalFunction (<function>, \"" << m_label << (m_loop == Loop::yes ? "\", Loop::yes)" : "\", Loop::no)");
    }

private:
    const std::function <void (AudioBuffer<SampleType>&)> m_signalFunction = nullptr;
    const std::string m_label;
    const Loop m_loop;
    std::shared_ptr<AudioBuffer<float>> m_userBuffer;
    size_t m_userBufferOffsetFrames = 0;
};

HART_SIGNAL_DECLARE_ALIASES_FOR (SignalFunction)

}  // namespace hart
