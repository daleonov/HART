#pragma once

#include <algorithm>  // max_element(), copy(), fill()
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
        other.erase();
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

        m_numChannels = other.m_numChannels;
        m_numFrames = other.m_numFrames;
        m_frames = std::move (other.m_frames);
        m_channelPointers = std::move (other.m_channelPointers);
        other.erase();

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

    void erase()
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

    // TODO: Implement resize() to avoid repeated memory re-allocations caused by spamming appendFrom()

    /// @brief Copies audio from another buffer
    /// @param destChannel Channel within this buffer to copy the frames to
    /// @param destStartFrame Start frame within this buffer's channel
    /// @param source Source buffer to read from
    /// @param sourceChannel Channel within the source buffer to read from
    /// @param sourceStartFrame Offset within the source buffer's channel to start reading frames from
    /// @param numFrames Number of frames to copy
    void copyFrom (size_t destChannel, size_t destStartFrame, const AudioBuffer& source, size_t sourceChannel, size_t sourceStartFrame, size_t numFrames)
    {
        if (destChannel >= m_numChannels || sourceChannel >= source.m_numChannels)
            HART_THROW_OR_RETURN_VOID (hart::IndexError, "Invalid channel");

        if (destStartFrame + numFrames > m_numFrames || sourceStartFrame + numFrames > source.m_numFrames)
            HART_THROW_OR_RETURN_VOID (hart::IndexError, "Invalid frame range");

        std::copy (
            source.m_channelPointers[sourceChannel] + sourceStartFrame,
            source.m_channelPointers[sourceChannel] + sourceStartFrame + numFrames,
            m_channelPointers[destChannel] + destStartFrame
            );
    }

    /// @brief Copies audio from another generic audio buffer
    /// @param destChannel Channel within this buffer to copy the frames to
    /// @param destStartFrame Start frame within this buffer's channel
    /// @param source Pointer to the source sample data, must contain at least `numFrames` samples
    /// @param numFrames Number of frames to copy
    void copyFrom (size_t destChannel, size_t destStartFrame, const SampleType* source, size_t numFrames)
    {
        if (destChannel >= m_numChannels)
            HART_THROW_OR_RETURN_VOID (hart::IndexError, "Invalid destination channel");

        if (destStartFrame + numFrames > m_numFrames)
            HART_THROW_OR_RETURN_VOID (hart::IndexError, "Invalid frame range");

        std::copy (source, source + numFrames, m_channelPointers[destChannel] + destStartFrame);
    }

    /// @brief Clears the entire buffer
    /// @details Sets all frames in all channels to zeros
    void clear()
    {
        std::fill (m_frames.begin(), m_frames.end(), (SampleType) 0);
    }

    /// @brief Clears a specific section of a given channel
    /// @details Overwrites a selected section of the channel with zeros
    /// @param channel Cnannel in which to clear a frame range
    /// @param startFrame Start of the frame range to clear (inclusive)
    /// @param numFrames Amount of frames to clear
    void clear (size_t channel, size_t startFrame, size_t numFrames)
    {
        if (channel >= m_numChannels)
            HART_THROW_OR_RETURN_VOID (hart::IndexError, "Invalid channel");

        if (startFrame + numFrames > m_numFrames)
            HART_THROW_OR_RETURN_VOID (hart::IndexError, "Invalid frame range");

        std::fill (m_channelPointers[channel], m_channelPointers[channel] + numFrames, (SampleType) 0);
    }

private:
    size_t m_numChannels = 0;
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
