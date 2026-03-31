#pragma once

#include <vector>

#include "hart_dsp.hpp"
#include "hart_precision.hpp"  // secPrecision
#include "hart_utils.hpp"  // roundToSizeT()

namespace hart
{

/// @brief Shifts an audio signal forward in time by a fixed amount
/// @details Delays the input signal by a specified duration, expressed in seconds.
/// Internally, the delay is converted to an integer number of frames based on the
/// current sample rate, so it soesn't do sub-frame (fractional) elays.
/// This DSP is a low-level utility intended for testing and signal alignment purposes,
/// such as compensating for latency in other processors. It performs a pure time shift
/// with no feedback or dry/wet mixing. The output is zero-filled until the internal
/// buffer is primed. Delay time is fixed and cannot be changed after construction or
/// copy/move assignment. The delay is identical across all channels, and processing
/// is only supported when the number of input and output channels matches.
/// It is encouraged to use it in conjunction with `hart::Impulse` signal,
/// although other signal types may also be appropriate.
/// @throws hart::ValueError for negative or zero delay (in seconds or frames)
/// @tparam SampleType Floating point sample type, typecally `float` or `double`.
/// @ingroup DSP
template <typename SampleType>
class TimeShift :
    public DSP<SampleType, TimeShift<SampleType>>
{
public:
    /// @brief Creates a TimeShift processor with a fixed delay.
    /// @param delaySeconds Delay duration in seconds. Must be greater than zero.
    /// The delay value is converted to an integer number of frames during prepare(),
    /// based on the provided sample rate. Values that resolve to zero frames are rejected.
    /// @throws hart::ValueError for negative or zero delay (in seconds or frames)
    TimeShift (double delaySeconds):
        m_delaySeconds (delaySeconds)
    {
        if (delaySeconds < 0.0 || floatsEqual (delaySeconds, 0.0))
            HART_THROW_OR_RETURN_VOID (hart::ValueError, "TimeShift: delaySeconds must be > 0");
    }

    /// @brief Creates a TimeShift processor by copying other one
    /// @details The entire state, included internal delay byffers' content is copied as well
    /// @param other TimeShift to copy from
    TimeShift (const TimeShift& other):
        DSP<SampleType, TimeShift<SampleType>> (other),
        m_delaySeconds (other.m_delaySeconds),
        m_delayFrames (other.m_delayFrames),
        m_bufferSizeFrames (other.m_bufferSizeFrames),
        m_writeIndex (other.m_writeIndex),
        m_buffers (other.m_buffers)
    {
    }

    /// @brief Creates a TimeShift processor by moving other one
    /// @details The entire state, included internal delay byffers' content is moved as well
    /// @param other TimeShift to move from
    TimeShift (TimeShift&& other) noexcept:
        DSP<SampleType, TimeShift<SampleType>> (std::move (other)),
        m_delaySeconds (other.m_delaySeconds),
        m_delayFrames (other.m_delayFrames),
        m_bufferSizeFrames (other.m_bufferSizeFrames),
        m_writeIndex (other.m_writeIndex),
        m_buffers (std::move (other.m_buffers))
    {
        other.m_delayFrames = 0;
        other.m_bufferSizeFrames = 0;
        other.m_writeIndex = 0;
    }

    /// @brief Creates a TimeShift processor by copying other one
    /// @details The entire state, included internal delay byffers' content is copied as well
    /// @param other TimeShift to copy from
    TimeShift& operator= (const TimeShift& other)
    {
        if (this == &other)
            return *this;

        DSP<SampleType, TimeShift<SampleType>>::operator= (other);

        m_delaySeconds = other.m_delaySeconds;
        m_delayFrames = other.m_delayFrames;
        m_bufferSizeFrames = other.m_bufferSizeFrames;
        m_writeIndex = other.m_writeIndex;
        m_buffers = other.m_buffers;

        return *this;
    }

    /// @brief Creates a TimeShift processor by moving other one
    /// @details The entire state, included internal delay byffers' content is moved as well
    /// @param other TimeShift to move from
    TimeShift& operator= (TimeShift&& other) noexcept
    {
        if (this == &other)
            return *this;

        DSP<SampleType, TimeShift<SampleType>>::operator= (std::move (other));

        m_delaySeconds = other.m_delaySeconds;
        m_delayFrames = other.m_delayFrames;
        m_bufferSizeFrames = other.m_bufferSizeFrames;
        m_writeIndex = other.m_writeIndex;
        m_buffers = std::move (other.m_buffers);

        other.m_delayFrames = 0;
        other.m_bufferSizeFrames = 0;
        other.m_writeIndex = 0;

        return *this;
    }

    ~TimeShift() override = default;

    void prepare (
        double sampleRateHz,
        size_t numInputChannels,
        size_t numOutputChannels,
        size_t /* maxBlockSizeFrames */
        ) override
    {
        hassert (numInputChannels == numOutputChannels)
        const size_t numChannels = numInputChannels;

        hassert (sampleRateHz > 0.0);
        m_delayFrames = roundToSizeT (m_delaySeconds * sampleRateHz);

        if (m_delayFrames == 0)
            HART_THROW_OR_RETURN_VOID (hart::ValueError, "TimeShift: Delay time resolved to 0 frames!");

        m_bufferSizeFrames = m_delayFrames + 1;
        m_writeIndex = 0;

        m_buffers.clear();
        m_buffers.resize (numChannels);

        for (size_t channel = 0; channel < numChannels; ++channel)
            m_buffers[channel].assign (m_bufferSizeFrames, static_cast<SampleType> (0));
    }

    void process (
        const AudioBuffer<SampleType>& input,
        AudioBuffer<SampleType>& output,
        const EnvelopeBuffers& /*envelopeBuffers*/,
        ChannelFlags /*channelsToProcess*/
        ) override
    {
        const size_t numFrames = input.getNumFrames();
        const size_t numChannels = input.getNumChannels();

        for (size_t frame = 0; frame < numFrames; ++frame)
        {
            const size_t readIndex = (m_writeIndex + m_bufferSizeFrames - m_delayFrames) % m_bufferSizeFrames;

            for (size_t channel = 0; channel < numChannels; ++channel)
            {
                output[channel][frame] = m_buffers[channel][readIndex];
                m_buffers[channel][m_writeIndex] = input[channel][frame];
            }

            m_writeIndex = (m_writeIndex + 1) % m_bufferSizeFrames;
        }
    }

    bool supportsEnvelopeFor (int /*id*/) const override
    {
        return false;
    }

    bool supportsChannelLayout (size_t numInputChannels, size_t numOutputChannels) const override
    {
        return numInputChannels == numOutputChannels;
    }

    bool supportsSampleRate (double sampleRateHz) const override
    {
        return sampleRateHz > 0.0;
    }

    void represent (std::ostream& stream) const override
    {
        stream << "TimeShift (" << secPrecision << m_delaySeconds << ")";
    }

    void setValue (int /*id*/, double /*value*/) override {}

    HART_DSP_COPYABLE (TimeShift);
    HART_DSP_MOVABLE (TimeShift);

private:
    double m_delaySeconds = 0.0;
    size_t m_delayFrames = 0;
    size_t m_bufferSizeFrames = 0;
    size_t m_writeIndex = 0;

    std::vector<std::vector<SampleType>> m_buffers;
};

HART_DSP_DECLARE_ALIASES_FOR (TimeShift)

} // namespace hart
