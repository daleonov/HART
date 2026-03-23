#pragma once

#include <algorithm>  // max_element(), copy(), fill()
#include <cmath>  // isnan()
#include <vector>

#include "hart_exceptions.hpp"
#include "hart_precision.hpp"  // hzPrecision
#include "hart_utils.hpp"  // nan(), floatsEqual()

/// @defgroup DataStructures Data Structures
/// @brief Custom data structures and containers

namespace hart {

/// @brief Container for audio data
/// @note This class owns a memory block with audio samples - so it's a container, not a view. Treat it like a heavyweight object.
/// @ingroup DataStructures
template <typename SampleType>
class AudioBuffer
{
public:
    /// @brief Creates an audio buffer
    /// @param numChannels Initial number of channels
    /// @param numFrames Numbers of frames (samples) to be allocated in each channel
    /// @param sampleRateHz Metadata for sample rate (in Hz) in which the data whould be interpreted, whenever applicable
    AudioBuffer (size_t numChannels = 0, size_t numFrames = 0, double sampleRateHz = nan<double>()) :
        m_numChannels (numChannels),
        m_numFrames (numFrames),
        m_sampleRateHz (sampleRateHz),
        m_frames (m_numChannels * m_numFrames),
        m_channelPointers (m_numChannels)
    {
        updateChannelPointers();
    }

    /// @brief Creates an audio buffer by copying
    AudioBuffer (const AudioBuffer& other) :
        m_numChannels (other.m_numChannels),
        m_numFrames (other.m_numFrames),
        m_sampleRateHz (other.m_sampleRateHz),
        m_frames (other.m_frames),
        m_channelPointers (m_numChannels)
    {
        updateChannelPointers();
    }

    /// @brief Creates an audio buffer by moving
    AudioBuffer (AudioBuffer&& other) :
        m_numChannels (other.m_numChannels),
        m_numFrames (other.m_numFrames),
        m_sampleRateHz (other.m_sampleRateHz),
        m_frames (std::move (other.m_frames)),
        m_channelPointers (std::move (other.m_channelPointers))
    {
        other.erase();
    }

    /// @brief The destructor
    ~AudioBuffer() = default;

    /// @brief Creates an audio buffer by copy-assigning
    AudioBuffer& operator= (const AudioBuffer& other)
    {
        if (this == &other)
            return *this;

        if (m_numChannels != other.m_numChannels)
            HART_THROW_OR_RETURN (hart::ChannelLayoutError, "Can't copy from a buffer with different number of channels", *this);

        m_numFrames = other.m_numFrames;
        m_sampleRateHz = other.m_sampleRateHz;
        m_frames = other.m_frames;
        m_channelPointers.resize (m_numChannels);
        updateChannelPointers();

        return *this;
    }

    /// @brief Creates an audio buffer by move-assigning
    AudioBuffer& operator= (AudioBuffer&& other)
    {
        if (this == &other)
            return *this;

        m_numChannels = other.m_numChannels;
        m_numFrames = other.m_numFrames;
        m_sampleRateHz = other.m_sampleRateHz;
        m_frames = std::move (other.m_frames);
        m_channelPointers = std::move (other.m_channelPointers);
        other.erase();

        return *this;
    }

    /// @brief Gets a raw pointer to the read-only audio data
    /// @return A pointer to the beginning of the audio data. Guaranteed to be a contiguous non-interleaved block of memory.
    const SampleType* const* getArrayOfReadPointers() const 
    {
        return static_cast<const SampleType* const*> (m_channelPointers.data());
    }

    /// @brief Gets a raw pointer to the mutable audio data
    /// @return A pointer to the beginning of the audio data. Guaranteed to be a contiguous non-interleaved block of memory.
    SampleType* const* getArrayOfWritePointers() 
    {
        return m_channelPointers.data();
    }

    /// @brief Creates an empty audio buffer with the same number of channels, frames and sample rate as the `other` buffer.
    /// @note The data from other buffer will not be copied, expect un-initialized values.
    /// @param other Reference buffer
    static AudioBuffer emptyLike (const AudioBuffer& other)
    {
        return other.hasSampleRate()
            ? AudioBuffer (other.getNumChannels(), other.getNumFrames(), other.getSampleRateHz())
            : AudioBuffer (other.getNumChannels(), other.getNumFrames());
    }

    /// @brief Get number of channels
    /// @return Number of allocated channels
    size_t getNumChannels() const { return m_numChannels; }

    /// @brief Get number of frames (samples)
    /// @return Number of allocated frames (samples) in every channel
    size_t getNumFrames() const { return m_numFrames; }

    /// @brief Resizes the buffer to hold a new number of frames per channel
    /// @details Resizing behaviour:
    ///   - If newNumFrames == current size, it won't do anything.
    ///   - If newNumFrames > current size, it will append silence (zeros) at the end.
    ///   - If newNumFrames < current size: it will jsut truncate, discarding samples from the end.
    /// Will always preserve existing channels and sample-rate metadata.
    /// @attention May cause reallocation, thus invalidating previous raw pointers!
    /// @param newNumFrames New number of frames per channel
    void setNumFrames (size_t newNumFrames)
    {
        if (newNumFrames == m_numFrames)
            return;

        const size_t oldTotalSamples = m_frames.size();
        const size_t newTotalSamples = m_numChannels * newNumFrames;

        if (newNumFrames < m_numFrames)
        {
            m_frames.resize (newTotalSamples);
            m_numFrames = newNumFrames;
            updateChannelPointers();
            return;
        }

        if (newTotalSamples > oldTotalSamples)
        {
            m_frames.resize (newTotalSamples);
            std::fill (
                m_frames.begin() + oldTotalSamples,
                m_frames.end(),
                SampleType (0)
            );
        }

        m_numFrames = newNumFrames;
        updateChannelPointers();
    }

    /// @brief Check if a specific sample rate was assigned to the audio buffer
    /// @note Unititialized buffers, as well as some specific Signal types, will not have a specific sample rate.
    /// @return `true` if there is a specific sample rate value, `false` otherwise
    bool hasSampleRate() const 
    {
        return ! std::isnan (m_sampleRateHz);
    }

    /// @brief Get a sample rate metadata
    /// @note Call `hasSampleRate()` if you're not sure if the buffer has a specific sample rate
    /// @return Buffer's sample rate in Hz
    double getSampleRateHz() const
    {
        hassert (! std::isnan (m_sampleRateHz));  // Call hasSampleRate() first! This buffer doesn't have a specific sample rate.
        return m_sampleRateHz;
    }

    /// @brief Get a duration of the audio buffer
    /// @note Before calling it, it's a good idea to call `hasSampleRate()` to check if the buffer has a specific sample rate, otherwise the duration is unknown.
    /// @return Duration of the audio buffer in seconds
    double getLengthSeconds() const
    {
        if (m_numFrames == 0)
            return 0.0;

        hassert (! std::isnan (m_sampleRateHz));  // This buffer doesn't have a specific sample rate, and hence the length is unknown
        hassert (! floatsEqual (m_sampleRateHz, 0.0));
        return static_cast <double> (m_numFrames) / m_sampleRateHz;
    }

    /// @brief Get a raw pointer to a specific channel's mutable audio data
    /// @note The data is guaranteed to have at least getNumFrames() amount of items, and guaranteed to be contiguus and non-interleaves block of memory
    /// @return Pointer to the audio data
    SampleType* operator[] (size_t channel)
    {
        return m_channelPointers[channel];
    }

    /// @brief Get a raw pointer to a specific channel's read-only audio data
    /// @note The data is guaranteed to have at least getNumFrames() amount of items, and guaranteed to be contiguus and non-interleaves block of memory
    /// @return Pointer to the audio data
    const SampleType* operator[] (size_t channel) const
    {
        return m_channelPointers[channel];
    }

    /// @brief Appends data from another buffer
    /// @warning This operation will resize the current buffer and potentially invalidate the previous raw data pointers returned by
    /// `getArrayOfReadPointers()`, `getArrayOfWritePointers()` and the "`[]`" operator, so make sure to keep those external pointers up to date.
    /// @param otherBuffer A buffer to append from
    void appendFrom (const AudioBuffer<SampleType>& otherBuffer)
    {
        if (otherBuffer.getNumChannels() != m_numChannels)
            HART_THROW_OR_RETURN_VOID (hart::ChannelLayoutError, "Channel count mismatch");

        // Not a huge deal, but appending a buffer with a different SR must be at least a little suspicious
        hassert ((! hasSampleRate() || ! otherBuffer.hasSampleRate()) || floatsEqual (getSampleRateHz(), otherBuffer.getSampleRateHz()));

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

    /// @brief Clears the buffer
    /// @details The number of frames after this operaion will be zero, but channel number will persist.
    void erase()
    {
        // Keeping the sample rate though, just wiping the data

        m_numFrames = 0;
        m_frames.clear();

        // If m_channelPointers was std::move'd, its size will be zero
        if (m_channelPointers.size() != m_numChannels)
            m_channelPointers.resize (m_numChannels);

        updateChannelPointers();
    }

    /// @brief Get the maximum absolute value in the buffer in a specific channel
    /// @param channel Channel of which the magnitude should be measured
    /// @param startFrame The beginnig of the range of the frames to look for magnitude
    /// @param numFrames Number of frames, beginning with startFrame, to look for the magnitude
    /// @return The maximum value, in linear domain (i.e. not decibels)
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

    /// @brief Get the maximum absolute value in the buffer across all channels
    /// @param startFrame The beginnig of the range of the frames to look for magnitude
    /// @param numFrames Number of frames, beginning with startFrame, to look for the magnitude
    /// @return The maximum value, in linear domain (i.e. not decibels)
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

        // Not a huge deal, but copying from a buffer with different SR must be at least a little suspicious
        hassert ((! hasSampleRate() || ! source.hasSampleRate()) || floatsEqual (getSampleRateHz(), source.getSampleRateHz()));

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
    /// @details Sets all frames in all channels to zeros, keeping the sample rate value intact
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

    /// @brief Prints readable representation of the audio buffer
    /// @param stream String stream to append the representation to
    void represent (std::ostream& stream) const
    {
        std::ostringstream oss;
        stream << "AudioBuffer (" << m_numChannels << ", " << m_numFrames;

        if (hasSampleRate())
            stream << ", " << hzPrecision << m_sampleRateHz << ')';
        else
            stream << ", nan())";
    }

private:
    size_t m_numChannels = 0;
    size_t m_numFrames = 0;
    double m_sampleRateHz = nan<double>();
    std::vector<SampleType> m_frames;
    std::vector<SampleType*> m_channelPointers;

    void updateChannelPointers()
    {
        for (size_t channel = 0; channel < m_numChannels; ++channel)
            m_channelPointers[channel] = m_numFrames > 0 ? &m_frames[channel * m_numFrames] : nullptr;
    }
};

}  // namespace hart
