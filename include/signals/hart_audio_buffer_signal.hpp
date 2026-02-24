#pragma once

#include "hart_audio_buffer.hpp"
#include "signals/hart_signal.hpp"
#include <memory>
#include <sstream>

// TODO: Better documentation

namespace hart {

/// @brief Plays audio from a pre-rendered AudioBuffer
/// @ingroup Signals
template<typename SampleType>
class AudioBufferSignal :
    public Signal<SampleType, AudioBufferSignal<SampleType>>
{
public:
    enum class Loop
    {
        yes,
        no
    };

    /// @brief Construct from a shared_ptr (shared ownership)
    explicit AudioBufferSignal (std::shared_ptr<AudioBuffer<SampleType>> buffer, Loop loop = Loop::no) :
        m_buffer (std::move (buffer)),
        m_loop (loop)
    {
        if (m_buffer == nullptr)
            HART_THROW_OR_RETURN_VOID (hart::NullPointerError, "Null pointer buffer shouldn't be used to make an AudioBufferSignal");

        this->setNumChannels (m_buffer->getNumChannels());
    }

    /// @brief Construct by copying a buffer (takes ownership of a copy)
    explicit AudioBufferSignal (const AudioBuffer<SampleType>& buffer, Loop loop = Loop::no) :
        m_buffer (std::make_shared<AudioBuffer<SampleType>> (buffer)),
        m_loop (loop)
    {
        this->setNumChannels (m_buffer->getNumChannels());
    }

    /// @brief Construct by moving a buffer (takes ownership)
    explicit AudioBufferSignal (AudioBuffer<SampleType>&& buffer, Loop loop = Loop::no) :
        m_buffer (std::make_shared<AudioBuffer<SampleType>> (std::move (buffer))),
        m_loop (loop)
    {
        this->setNumChannels (m_buffer->getNumChannels());
    }

    bool supportsNumChannels (size_t numChannels) const override
    {
        return m_buffer != nullptr && numChannels == m_buffer->getNumChannels();
    }

    bool supportsSampleRate(double sampleRateHz) const override
    {
        if (m_buffer == nullptr)
            return false;

        // If buffer has undefined sample rate, we'll accept any rate
        return ! m_buffer->hasSampleRate() || m_buffer->getSampleRateHz() == sampleRateHz;
    }

    void prepare (double sampleRateHz, size_t numOutputChannels, size_t /*maxBlockSizeFrames*/) override
    {
        if (m_buffer == nullptr)
            HART_THROW_OR_RETURN_VOID (hart::NullPointerError, "AudioBufferSignal: buffer is null");

        if (numOutputChannels != m_buffer->getNumChannels())
            HART_THROW_OR_RETURN_VOID (hart::ChannelLayoutError, "AudioBufferSignal: output channel count does not match buffer");

        if (m_buffer->hasSampleRate() && m_buffer->getSampleRateHz() != sampleRateHz)
            HART_THROW_OR_RETURN_VOID(hart::SampleRateError, "AudioBufferSignal: sample rate does not match buffer");
    }

    void renderNextBlock (AudioBuffer<SampleType>& output) override
    {
        if (m_buffer == nullptr)
        {
            hassertfalse;  // No buffer to play audio from!
            output.clear();
            return;
        }

        const size_t numFrames = output.getNumFrames();
        const size_t bufferTotalFrames = m_buffer->getNumFrames();
        const size_t bufferNumChannels = m_buffer->getNumChannels();
        size_t framesWritten = 0;

        while (framesWritten < numFrames)
        {
            if (m_readPosition >= bufferTotalFrames)
            {
                if (m_loop == Loop::no)
                    break;  // fill remaining with zeros below

                m_readPosition = 0;  // loop
            }

            const size_t framesAvailable = bufferTotalFrames - m_readPosition;
            const size_t framesToCopy = std::min (framesAvailable, numFrames - framesWritten);

            for (size_t channel = 0; channel < bufferNumChannels; ++channel)
            {
                const SampleType* src = (*m_buffer)[channel] + m_readPosition;
                SampleType* dst = output[channel] + framesWritten;
                std::copy(src, src + framesToCopy, dst);
            }

            framesWritten += framesToCopy;
            m_readPosition += framesToCopy;
        }

        // Zero out any remaining frames (only when non‑looping and we ran out)
        for (; framesWritten < numFrames; ++framesWritten)
            for (size_t channel = 0; channel < bufferNumChannels; ++channel)
                output[channel][framesWritten] = SampleType(0);
    }

    void reset() override
    {
        m_readPosition = 0;
    }

    void represent (std::ostream& stream) const override
    {
        stream << "AudioBufferSignal (";

        if (m_buffer != nullptr)
            m_buffer->represent (stream);
        else
            stream << "<nullptr>";

        stream << ", Loop::" << (m_loop == Loop::yes ? "yes" : "no") << ')';
    }

    std::unique_ptr<SignalBase<SampleType>> copy() const override
    {
        return hart::make_unique<AudioBufferSignal> (m_buffer, m_loop);
    }

    std::unique_ptr<SignalBase<SampleType>> move() override
    {
        return hart::make_unique<AudioBufferSignal> (std::move (m_buffer), m_loop);
    }

private:
    std::shared_ptr<AudioBuffer<SampleType>> m_buffer;
    Loop m_loop = Loop::no;
    size_t m_readPosition = 0;
};

HART_SIGNAL_DECLARE_ALIASES_FOR (AudioBufferSignal)

} // namespace hart
