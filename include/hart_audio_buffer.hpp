#pragma once

#include<vector>

namespace hart {

template <typename SampleType>
class AudioBuffer
{
public:
    AudioBuffer (size_t numChannels = 0, size_t numFrames = 0) :
        m_numChannels (numChannels),
        m_numFrames (numFrames),
        m_frames (m_numChannels * m_numFrames),
        m_channelPtrs (m_numChannels)
    {
        for (size_t channel = 0; channel < m_numChannels; ++channel)
            m_channelPtrs[channel] = &m_frames[channel * m_numFrames];
    }

    const SampleType* const* getArrayOfReadPointers() const 
    {
        return static_cast<const SampleType* const*> (m_channelPtrs.data());
    }

    SampleType* const* getArrayOfWritePointers() 
    {
        return m_channelPtrs.data();
    }

private:
    const size_t m_numChannels = 0;
    const size_t m_numFrames = 0;
    std::vector<SampleType> m_frames;
    std::vector<SampleType*> m_channelPtrs;
};

}  // namespace hart
