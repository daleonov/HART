#pragma once

#include <vector>

#include "hart_exceptions.hpp"

namespace hart {

template <typename SampleType>
class AudioBuffer
{
public:
    AudioBuffer (size_t numChannels = 0, size_t numFrames = 0) :
        m_numChannels (numChannels),
        m_numFrames (numFrames),
        m_frames (m_numChannels * m_numFrames),
        m_channelPointers (m_numChannels)
    {
        updateChannelPointers();
    }

    AudioBuffer(const AudioBuffer& other) :
        m_numChannels (other.m_numChannels),
        m_numFrames (other.m_numFrames),
        m_frames (other.m_frames),
        m_channelPointers (m_numChannels)
    {
        updateChannelPointers();
    }

    AudioBuffer (AudioBuffer&& other) :
        m_numChannels (other.m_numChannels),
        m_numFrames (other.m_numFrames),
        m_frames (std::move (other.m_frames)),
        m_channelPointers (std::move (other.m_channelPointers))
    {
        other.clear();
    }

    ~AudioBuffer() = default;

    AudioBuffer& operator= (const AudioBuffer& other)
    {
        if (this == &other)
            return *this;

        if (m_numChannels != other.m_numChannels)
            HART_THROW_OR_RETURN (hart::ChannelLayoutError, "Can't copy from a buffer with different number of channels", *this);

        m_numFrames = other.m_numFrames;
        m_frames = other.m_frames;
        m_channelPointers.resize (m_numChannels);
        updateChannelPointers();

        return *this;
    }

    AudioBuffer& operator= (AudioBuffer&& other)
    {
        if (this == &other)
            return *this;

        if (m_numChannels != other.m_numChannels)
            HART_THROW_OR_RETURN (hart::ChannelLayoutError, "Can't move from a buffer with different number of channels", *this);

        m_numFrames = other.m_numFrames;
        m_frames = std::move (other.m_frames);
        m_channelPointers = std::move (other.m_channelPointers);
        other.clear();

        return *this;
    }

    const SampleType* const* getArrayOfReadPointers() const 
    {
        return static_cast<const SampleType* const*> (m_channelPointers.data());
    }

    SampleType* const* getArrayOfWritePointers() 
    {
        return m_channelPointers.data();
    }

    static AudioBuffer emptyLike (const AudioBuffer& other)
    {
        return AudioBuffer (other.getNumChannels(), other.getNumFrames());
    }

    size_t getNumChannels() const { return m_numChannels; }
    size_t getNumFrames() const { return m_numFrames; }

    SampleType* operator[] (size_t channel)
    {
        return m_channelPointers[channel];
    }

    const SampleType* operator[] (size_t channel) const
    {
        return m_channelPointers[channel];
    }

    void appendFrom (const AudioBuffer<SampleType>& otherBuffer)
    {
        if (otherBuffer.getNumChannels() != m_numChannels)
            HART_THROW_OR_RETURN_VOID (hart::ChannelLayoutError, "Channel count mismatch");

        const size_t thisNumFrames = m_numFrames;
        const size_t otherNumFrames = otherBuffer.getNumFrames();

        std::vector<SampleType> combinedFrames (m_numChannels * (thisNumFrames + otherNumFrames));

        for (size_t channel = 0; channel < m_numChannels; ++channel)
        {
            SampleType* newChannelStart = &combinedFrames[channel * (thisNumFrames + otherNumFrames)];
            std::copy (m_channelPointers[channel], m_channelPointers[channel] + thisNumFrames, newChannelStart);
            std::copy (otherBuffer[channel], otherBuffer[channel] + otherNumFrames, newChannelStart + thisNumFrames);
        }

        m_frames = std::move (combinedFrames);
        m_numFrames += otherNumFrames;

        updateChannelPointers();
    }

    void clear()
    {
        m_numFrames = 0;
        m_frames.clear();

        // If m_channelPointers was std::move'd, its size will be zero
        if (m_channelPointers.size() != m_numChannels)
            m_channelPointers.resize (m_numChannels);

        updateChannelPointers();
    }

    SampleType getMagnitude (size_t channel, size_t startFrame, size_t numFrames) const
    {
        if (channel >= m_numChannels)
            HART_THROW_OR_RETURN (hart::IndexError, "Invalid channel", (SampleType) 0);

        if (startFrame + numFrames > m_numFrames || numFrames == 0)
            HART_THROW_OR_RETURN (hart::IndexError, "Invalid frame range", (SampleType) 0);

        const SampleType* start = m_channelPointers[channel] + startFrame;
        const SampleType* peakSample = std::max_element (
            start,
            start + numFrames,
            [] (SampleType a, SampleType b) { return std::abs (a) < std::abs (b); }
            );

        return std::abs (*peakSample);
    }

    SampleType getMagnitude (size_t startFrame, size_t numFrames) const
    {
        if (startFrame + numFrames > m_numFrames || numFrames == 0)
                HART_THROW_OR_RETURN (hart::IndexError, "Invalid frame range", (SampleType) 0);

        SampleType peakSampleAcrossAllChannels = (SampleType) 0;

        for (size_t channel = 0; channel < m_numChannels; ++channel)
        {
            const SampleType* start = m_channelPointers[channel] + startFrame;
            const SampleType* peakSample = std::max_element (
                start,
                start + numFrames,
                [] (SampleType a, SampleType b) { return std::abs (a) < std::abs (b); }
                );
            peakSampleAcrossAllChannels = std::max (peakSampleAcrossAllChannels, std::abs (*peakSample));
        }

        return peakSampleAcrossAllChannels;
    }

    // TODO: Implement resize() and copyFrom() to avoid repeated memory re-allocations caused by spamming appendFrom()

private:
    const size_t m_numChannels = 0;
    size_t m_numFrames = 0;
    std::vector<SampleType> m_frames;
    std::vector<SampleType*> m_channelPointers;

    void updateChannelPointers()
    {
        for (size_t channel = 0; channel < m_numChannels; ++channel)
            m_channelPointers[channel] = m_numFrames > 0 ? &m_frames[channel * m_numFrames] : nullptr;
    }
};

}  // namespace hart
