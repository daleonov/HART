#pragma once

#include <vector>
#include <cmath>
#include <stdexcept>

#include "hart_dsp.hpp"
#include "hart_precision.hpp"  // secPrecision
#include "hart_utils.hpp"  // roundToSizeT()

// TODO: Add documentation

namespace hart
{

template <typename SampleType>
class TimeShift :
    public DSP<SampleType, TimeShift<SampleType>>
{
public:
    TimeShift (double delaySeconds):
        m_delaySeconds (delaySeconds)
    {
        if (delaySeconds < 0.0 || floatsEqual (delaySeconds, 0.0))
            HART_THROW_OR_RETURN_VOID (hart::ValueError, "TimeShift: delaySeconds must be > 0");
    }

    TimeShift (const TimeShift& other):
        DSP<SampleType, TimeShift<SampleType>> (other),
        m_delaySeconds (other.m_delaySeconds),
        m_delayFrames (other.m_delayFrames),
        m_bufferSizeFrames (other.m_bufferSizeFrames),
        m_writeIndex (other.m_writeIndex),
        m_buffers (other.m_buffers)
    {
    }

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
        sampleRateHz = sampleRateHz;

        m_delayFrames = roundToSizeT (m_delaySeconds * sampleRateHz);

        if (m_delayFrames == 0)
            HART_THROW_OR_RETURN_VOID (hart::ValueError, "TimeShift: Delay time resolved to 0 frames!");

        m_bufferSizeFrames = m_delayFrames;
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

    void setValue (int /*id*/, double /*value*/) {}

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
