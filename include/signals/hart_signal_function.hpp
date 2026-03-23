#pragma once

#include <algorithm>
#include <functional>
#include <memory>

#include "signals/hart_signal.hpp"
#include "hart_utils.hpp"  // floatsEqual()

// TODO: Add docs
// TODO: Allow user buffer mismatch, if it's mono, and do multiplexing?

namespace hart
{

/// @ingroup Signals
template<typename SampleType>
class SignalFunction:
    public Signal<SampleType, SignalFunction<SampleType>>
{
public:
    enum class Loop
    {
        yes,
        no
    };

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
