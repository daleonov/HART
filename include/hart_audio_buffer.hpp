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
            throw hart::ChannelMismatchException ("Channel count mismatch");

        const size_t thisNumFrames = m_numFrames;
        const size_t otherNumFrames = otherBuffer.getNumFrames();

        std::vector<SampleType> combinedFrames (m_numChannels * (thisNumFrames + otherNumFrames));

        for (size_t channel = 0; channel < m_numChannels; ++channel)
        {
            const SampleType* newChannelStart = &combinedFrames[channel * (thisNumFrames + otherNumFrames)];
            std::copy (m_channelPointers[channel], m_channelPointers[channel] + thisNumFrames, newChannelStart);
            std::copy (otherBuffer[channel], otherBuffer[channel] + otherNumFrames, newChannelStart + thisNumFrames);
        }

        m_frames = std::move (combinedFrames);
        m_numFrames += otherNumFrames;

        updateChannelPointers();
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
            m_channelPointers[channel] = &m_frames[channel * m_numFrames];
    }
};

}  // namespace hart
