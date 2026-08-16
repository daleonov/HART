#pragma once

#include <algorithm>  // max_element(), copy(), fill()
#include <cmath>  // isnan(), abs()
#include <utility>  // pair, make_pair()
#include <vector>

#include "hart_exceptions.hpp"
#include "hart_precision.hpp"  // hzPrecision
#include "hart_preparation.hpp"
#include "hart_resample.hpp"
#include "hart_slice.hpp"
#include "hart_utils.hpp"  // nan(), floatsEqual(), roundToSizeT()

/// @defgroup DataStructures Data Structures
/// @brief Custom data structures and containers

namespace hart {

template <typename T> class DSPBase;
template <typename T> class SignalBase;

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
    AudioBuffer (AudioBuffer&& other) noexcept :
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
    AudioBuffer& operator= (AudioBuffer&& other) noexcept
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
    /// @return A pointer to an array of per-channel read pointers.
    /// It is guaranteed that each channel points to a contiguous non-interleaved block of memory.
    const SampleType* const* getArrayOfReadPointers() const 
    {
        return static_cast<const SampleType* const*> (m_channelPointers.data());
    }

    /// @brief Gets a raw pointer to the mutable audio data
    /// @return A pointer to an array of per-channel write pointers.
    /// It is guaranteed that each channel points to a contiguous non-interleaved block of memory.
    SampleType* const* getArrayOfWritePointers() 
    {
        return m_channelPointers.data();
    }

    /// @brief Creates an empty audio buffer with the same number of channels, frames and sample rate as the `other` buffer.
    /// @note The data from the other buffer will not be copied. Newly allocated samples will be value-initialized.
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
    ///   - If newNumFrames < current size: it will just truncate, discarding samples from the end.
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
    /// @note Uninitialized buffers, as well as some specific Signal types, will not have a specific sample rate.
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
    /// @note The data is guaranteed to have at least `getNumFrames()` items and to be a contiguous non-interleaved block of memory.
    /// @return Pointer to the audio data
    SampleType* operator[] (size_t channel)
    {
        return m_channelPointers[channel];
    }

    /// @brief Get a raw pointer to a specific channel's read-only audio data
    /// @note The data is guaranteed to have at least `getNumFrames()` items and to be a contiguous non-interleaved block of memory.
    /// @return Pointer to the audio data
    const SampleType* operator[] (size_t channel) const
    {
        return m_channelPointers[channel];
    }

    /// @brief Checks whether this buffer contains approximately the same audio as another buffer
    /// @details The comparison is relaxed - each pair of corresponding samples must differ by
    /// no more than `toleranceLinear`, but not bit-exact. Both buffers must have the same number
    /// of channels and frames. Also, if both buffers have sample-rate metadata, the sample rates
    /// must match. If both buffers have no sample-rate metadata assigned, they will still be
    /// considered equal as long as their dimensions match, and sample values match within tolerance.
    /// @param other Buffer to compare against
    /// @param toleranceLinear Absolute sample tolerance in linear domain (not decibels)
    /// @return `true` if the buffers are equal within the specified tolerance, `false` otherwise
    bool equalsTo (const AudioBuffer& other, SampleType toleranceLinear = (SampleType) 1e-6) const
    {
        if (m_numChannels != other.m_numChannels || m_numFrames != other.m_numFrames)
            return false;

        if (hasSampleRate() && other.hasSampleRate() && ! floatsEqual (m_sampleRateHz, other.m_sampleRateHz))
            return false;

        const SampleType* thisRawData = m_frames.data();
        const SampleType* otherRawData = other.m_frames.data();
        const size_t numFlattenedSamples = m_frames.size();

        for (size_t sample = 0; sample < numFlattenedSamples; ++sample)
            if (std::abs (thisRawData[sample] - otherRawData[sample]) > toleranceLinear)
                return false;

        return true;
    }

    /// @brief Checks whether two buffers contain approximately the same audio
    /// @details Equivalent to calling `AudioBuffer::equalsTo (other)` with the default tolerance
    /// @param other Buffer to compare against
    /// @return `true` if the buffers are equal within the default tolerance, `false` otherwise
    bool operator== (const AudioBuffer& other) const
    {
        return equalsTo (other);
    }

    /// @brief Checks whether two buffers differ beyond the default comparison tolerance
    /// @details Equivalent to `! AudioBuffer::equalsTo (other)`
    /// @param other Buffer to compare against
    /// @return `true` if the buffers are not equal within the default tolerance, `false` otherwise
    bool operator!= (const AudioBuffer& other) const
    {
        return ! equalsTo (other);
    }

    /// @brief Creates a new buffer as a sample-wise sum of two existing ones
    /// @param other Buffer to sum with
    /// @return A new buffer that contains a sample-wise sum of two buffers
    /// @throws hart::SizeError is buffer lengths (in frames) are mismatched
    /// @throws hart::ChannelLayoutError is buffer cumbers of channels are mismatched
    /// @throws hart::SampleRateError is buffer sample rates are mismatched.
    /// However, if neither of buffers has a sample rate assigned, this operation is allowed.
    AudioBuffer<SampleType> operator+ (const AudioBuffer& other) const
    {
        if (getNumFrames() != other.getNumFrames())
            HART_THROW_OR_RETURN (hart::SizeError, "Cannot add two buffers of mismatched lengths", {});
        
        if (getNumChannels() != other.getNumChannels())
            HART_THROW_OR_RETURN (hart::ChannelLayoutError, "Cannot add two buffers of mismatched numbers of channels", {});

        const bool buffersHaveMismatchedSampleRates = hasSampleRate() && other.hasSampleRate() && hart::floatsNotEqual (getSampleRateHz(), other.getSampleRateHz());
        const bool oneBufferHasSampleRateButOtherDoesnt = hasSampleRate() ^ other.hasSampleRate();

        // This could be a legit use case, but still not very clean, so let's not allow it until we have a good reason to let it happen
        if (buffersHaveMismatchedSampleRates || oneBufferHasSampleRateButOtherDoesnt)
            HART_THROW_OR_RETURN (hart::SampleRateError, "Cannot add two buffers of mismatched sample rates", {});

        AudioBuffer<SampleType> tmp (*this);
        hassert (tmp.m_frames.size() == other.m_frames.size());

        SampleType* rawDataA = tmp.m_frames.data();
        const SampleType* rawDataB = other.m_frames.data();

        for (size_t sample = 0; sample < tmp.m_frames.size(); ++sample)
            rawDataA[sample] += rawDataB[sample];
        
        return tmp;
    }

    /// @brief Creates a new buffer as a multiplying each sample of existing number by a provided gain value
    /// @param other gainLinear Gain to multiply each sample by
    /// @return A new buffer that contains a samples of existing buffer, multiplied by the provided gain
    AudioBuffer<SampleType> operator* (SampleType gainLinear) const
    {
        AudioBuffer<SampleType> tmp (*this);
        SampleType* rawData = tmp.m_frames.data();

        for (size_t sample = 0; sample < tmp.m_frames.size(); ++sample)
            rawData[sample] *= gainLinear;
        
        return tmp;
    }

    /// @brief Creates a new buffer as a sample-wise difference of two existing ones
    /// @param other Buffer to subtract from with
    /// @return A new buffer that contains a sample-wise difference of two buffers
    /// @throws hart::SizeError is buffer lengths (in frames) are mismatched
    /// @throws hart::ChannelLayoutError is buffer cumbers of channels are mismatched
    /// @throws hart::SampleRateError is buffer sample rates are mismatched.
    /// However, if neither of buffers has a sample rate assigned, this operation is allowed.
    AudioBuffer<SampleType> operator- (const AudioBuffer& other) const
    {
        return *this + (other * SampleType (-1));
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
    /// @details The number of frames after this operation will be zero, but channel number will persist.
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
    /// @param startFrame The beginning of the range of the frames to look for magnitude
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
    /// @param startFrame The beginning of the range of the frames to look for magnitude
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
    /// @param channel Channel in which to clear a frame range
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
        stream << "AudioBuffer (" << m_numChannels << ", " << m_numFrames;

        if (hasSampleRate())
            stream << ", " << hzPrecision << m_sampleRateHz << "_Hz)";
        else
            stream << ", nan())";
    }

    /// @brief Fills this entire AudioBuffer with a specific single value
    /// @param value Value to assign every frame in the buffer to
    /// @return A reference to this AudioBuffer for chaining further operations.
    AudioBuffer<SampleType>& fillWith (SampleType value)
    {
        std::fill(m_frames.begin(), m_frames.end(), value);
        return *this;
    }

    /// @brief Fills this AudioBuffer by rendering audio from a Signal.
    /// @details
    /// This method uses the supplied Signal and renders audio into this buffer, replacing all existing samples.
    /// The Signal is not consumed and can be re-used after rendering completes.
    ///
    /// The buffer's sample rate and channel layout are used as the hosting configuration.
    /// If the Signal does not support the current channel count or sample rate, an exception is raised.
    ///
    /// Rendering is performed block-by-block when @p blockSizeFrames is smaller than the buffer length.
    /// The final block may be shorter than the requested block size.
    /// @param signal A reusable Signal object to render from.
    /// @param blockSizeFrames Number of frames to render per block. Pass 0 to render the whole buffer in one go.
    /// @param signalPreparation Controls whether the Signal should be reset and/or prepared before rendering.
    /// @return A reference to this AudioBuffer for chaining further operations.
    /// @throws hart::SampleRateError if Sample rate of this buffer is undefined, or unsupported by the Signal.
    /// @throws hart::ChannelLayoutError if this buffer has no channels allocated, or number of channels
    /// is unsupported by the Signal
    /// @throws hart::SizeError is this buffer has no frames allocated
    AudioBuffer<SampleType>& fillWith (SignalBase<SampleType>& signal, size_t blockSizeFrames = 0, Preparation signalPreparation = Preparation::resetAndPrepare) &
    {
        _fillWith (signal, blockSizeFrames, signalPreparation);
        return *this;
    }

    /// @brief Fills this AudioBuffer by rendering audio from a Signal.
    /// @details
    /// This method uses the supplied Signal and renders audio into this buffer, replacing all existing samples.
    ///
    /// The buffer's sample rate and channel layout are used as the hosting configuration.
    /// If the Signal does not support the current channel count or sample rate, an exception is raised.
    ///
    /// Rendering is performed block-by-block when @p blockSizeFrames is smaller than the buffer length.
    /// The final block may be shorter than the requested block size.
    /// @param signal A temporary Signal instance to render from.
    /// @param blockSizeFrames Number of frames to render per block. Pass 0 to render the whole buffer in one go.
    /// @param signalPreparation Controls whether the Signal should be reset and/or prepared before rendering.
    /// @return A reference to this AudioBuffer for chaining further operations.
    /// @throws hart::SampleRateError if Sample rate of this buffer is undefined, or unsupported by the Signal.
    /// @throws hart::ChannelLayoutError if this buffer has no channels allocated, or number of channels
    /// is unsupported by the Signal
    /// @throws hart::SizeError is this buffer has no frames allocated
    AudioBuffer<SampleType>& fillWith (SignalBase<SampleType>&& signal, size_t blockSizeFrames = 0, Preparation signalPreparation = Preparation::resetAndPrepare) &
    {
        _fillWith (signal, blockSizeFrames, signalPreparation);
        return *this;
    }

    /// @brief Fills this AudioBuffer by rendering audio from a Signal.
    /// @details
    /// This method uses the supplied Signal and renders audio into this buffer, replacing all existing samples.
    /// The Signal is not consumed and can be re-used after rendering completes.
    ///
    /// The buffer's sample rate and channel layout are used as the hosting configuration.
    /// If the Signal does not support the current channel count or sample rate, an exception is raised.
    ///
    /// Rendering is performed block-by-block when @p blockSizeFrames is smaller than the buffer length.
    /// The final block may be shorter than the requested block size.
    /// @param signal A reusable Signal object to render from.
    /// @param blockSizeFrames Number of frames to render per block. Pass 0 to render the whole buffer in one go.
    /// @param signalPreparation Controls whether the Signal should be reset and/or prepared before rendering.
    /// @return An rvalue reference to this AudioBuffer, allowing fluent chaining with temporary buffers.
    /// @throws hart::SampleRateError if Sample rate of this buffer is undefined, or unsupported by the Signal.
    /// @throws hart::ChannelLayoutError if this buffer has no channels allocated, or number of channels
    /// is unsupported by the Signal
    /// @throws hart::SizeError is this buffer has no frames allocated
    AudioBuffer<SampleType>&& fillWith (SignalBase<SampleType>& signal, size_t blockSizeFrames = 0, Preparation signalPreparation = Preparation::resetAndPrepare) &&
    {
        _fillWith (signal, blockSizeFrames, signalPreparation);
        return std::move (*this);
    }

    /// @brief Fills this AudioBuffer by rendering audio from a Signal.
    /// @details
    /// This method uses the supplied Signal and renders audio into this buffer, replacing all existing samples.
    ///
    /// The buffer's sample rate and channel layout are used as the hosting configuration.
    /// If the Signal does not support the current channel count or sample rate, an exception is raised.
    ///
    /// Rendering is performed block-by-block when @p blockSizeFrames is smaller than the buffer length.
    /// The final block may be shorter than the requested block size.
    /// @param signal A temporary Signal instance to render from.
    /// @param blockSizeFrames Number of frames to render per block. Pass 0 to render the whole buffer in one go.
    /// @param signalPreparation Controls whether the Signal should be reset and/or prepared before rendering.
    /// @return An rvalue reference to this AudioBuffer, allowing fluent chaining with temporary buffers.
    /// @throws hart::SampleRateError if Sample rate of this buffer is undefined, or unsupported by the Signal.
    /// @throws hart::ChannelLayoutError if this buffer has no channels allocated, or number of channels
    /// is unsupported by the Signal
    /// @throws hart::SizeError is this buffer has no frames allocated
    AudioBuffer<SampleType>&& fillWith (SignalBase<SampleType>&& signal, size_t blockSizeFrames = 0, Preparation signalPreparation = Preparation::resetAndPrepare) &&
    {
        _fillWith (signal, blockSizeFrames, signalPreparation);
        return std::move (*this);
    }

    /// @brief Processes this AudioBuffer by rendering its audio through a provided DSP
    /// @details
    /// This method uses the supplied DSP and renders audio from this buffer, replacing all existing samples.
    /// The DSP is not consumed and can be re-used after rendering completes.
    ///
    /// The buffer's sample rate and channel layout are used as the hosting configuration.
    /// If the DSP does not support the current channel count or sample rate, an exception is raised.
    ///
    /// Rendering is performed block-by-block when @p blockSizeFrames is smaller than the buffer length.
    /// The final block may be shorter than the requested block size.
    /// @param signal A reusable hart::DSP object to render from
    /// @param blockSizeFrames Number of frames to render per block. Pass 0 to render the whole buffer in one go
    /// @param signalPreparation Controls whether the DSP object should be reset and/or prepared before rendering
    /// @return A reference to this AudioBuffer for chaining further operations
    /// @throws hart::SampleRateError if Sample rate of this buffer is undefined, or unsupported by the DSP object
    /// @throws hart::ChannelLayoutError if this buffer has no channels allocated, or number of channels
    /// is unsupported by the DSP object
    /// @throws hart::SizeError is this buffer has no frames allocated
    AudioBuffer<SampleType>& processWith (DSPBase<SampleType>& dsp, size_t blockSizeFrames = 0, Preparation dspPreparation = Preparation::resetAndPrepare) &
    {
        _processWith (dsp, blockSizeFrames, dspPreparation);
        return *this;
    }

    /// @brief Processes this AudioBuffer by rendering its audio through a provided DSP
    /// @details
    /// This method uses the supplied DSP and renders audio from this buffer, replacing all existing samples
    ///
    /// The buffer's sample rate and channel layout are used as the hosting configuration.
    /// If the DSP does not support the current channel count or sample rate, an exception is raised.
    ///
    /// Rendering is performed block-by-block when @p blockSizeFrames is smaller than the buffer length
    /// The final block may be shorter than the requested block size
    /// @param signal A temporary hart::DSP object to process AudioBuffer's contents with
    /// @param blockSizeFrames Number of frames to render per block. Pass 0 to render the whole buffer in one go.
    /// @param signalPreparation Controls whether the DSP object should be reset and/or prepared before rendering
    /// @return A reference to this AudioBuffer for chaining further operations
    /// @throws hart::SampleRateError if Sample rate of this buffer is undefined, or unsupported by the DSP object
    /// @throws hart::ChannelLayoutError if this buffer has no channels allocated, or number of channels
    /// is unsupported by the DSP object
    /// @throws hart::SizeError is this buffer has no frames allocated
    AudioBuffer<SampleType>& processWith (DSPBase<SampleType>&& dsp, size_t blockSizeFrames = 0, Preparation dspPreparation = Preparation::resetAndPrepare) &
    {
        _processWith (dsp, blockSizeFrames, dspPreparation);
        return *this;
    }

    /// @brief Processes this AudioBuffer by rendering its audio through a provided DSP
    /// @details
    /// This method uses the supplied DSP and renders audio from this buffer, replacing all existing samples.
    /// The DSP is not consumed and can be re-used after rendering completes.
    ///
    /// The buffer's sample rate and channel layout are used as the hosting configuration.
    /// If the DSP does not support the current channel count or sample rate, an exception is raised.
    ///
    /// Rendering is performed block-by-block when @p blockSizeFrames is smaller than the buffer length.
    /// The final block may be shorter than the requested block size.
    /// @param signal A reusable hart::DSP object to render from
    /// @param blockSizeFrames Number of frames to render per block. Pass 0 to render the whole buffer in one go.
    /// @param signalPreparation Controls whether the DSP object should be reset and/or prepared before rendering.
    /// @return An rvalue reference to this AudioBuffer, allowing fluent chaining with temporary buffers
    /// @throws hart::SampleRateError if Sample rate of this buffer is undefined, or unsupported by the DSP object
    /// @throws hart::ChannelLayoutError if this buffer has no channels allocated, or number of channels
    /// is unsupported by the DSP object
    /// @throws hart::SizeError is this buffer has no frames allocated
    AudioBuffer<SampleType>&& processWith (DSPBase<SampleType>& dsp, size_t blockSizeFrames = 0, Preparation dspPreparation = Preparation::resetAndPrepare) &&
    {
        _processWith (dsp, blockSizeFrames, dspPreparation);
        return std::move (*this);
    }

    /// @brief Processes this AudioBuffer by rendering its audio through a provided DSP
    /// @details
    /// This method uses the supplied DSP and renders audio from this buffer, replacing all existing samples.
    ///
    /// The buffer's sample rate and channel layout are used as the hosting configuration.
    /// If the DSP does not support the current channel count or sample rate, an exception is raised.
    ///
    /// Rendering is performed block-by-block when @p blockSizeFrames is smaller than the buffer length.
    /// The final block may be shorter than the requested block size.
    /// @param signal A temporary hart::DSP object to process AudioBuffer's contents with
    /// @param blockSizeFrames Number of frames to render per block. Pass 0 to render the whole buffer in one go.
    /// @param signalPreparation Controls whether the DSP object should be reset and/or prepared before rendering
    /// @return An rvalue reference to this AudioBuffer, allowing fluent chaining with temporary buffers
    /// @throws hart::SampleRateError if Sample rate of this buffer is undefined, or unsupported by the DSP object
    /// @throws hart::ChannelLayoutError if this buffer has no channels allocated, or number of channels
    /// is unsupported by the DSP object
    /// @throws hart::SizeError is this buffer has no frames allocated
    AudioBuffer<SampleType>&& processWith (DSPBase<SampleType>&& dsp, size_t blockSizeFrames = 0, Preparation dspPreparation = Preparation::resetAndPrepare) &&
    {
        _processWith (dsp, blockSizeFrames, dspPreparation);
        return std::move (*this);
    }

    /// @brief Returns a pair of indices representing a provided slice
    /// @param slice A Slice instance. Valid slice types are `Slice::Type::whole`
    /// `Slice::Type::frames` and `Slice::Type::time`
    /// @retval first Index representing the beginning of the range, inclusive
    /// @retval second Index representing the end of the range, non-inclusive
    std::pair<size_t, size_t> getFrameIndices (const Slice& slice) const
    {
        const size_t numFrames = getNumFrames();

        switch (slice.type)
        {
            case Slice::Type::whole:
            {
                return {0, numFrames};
            }

            case Slice::Type::frames:
            {
                const size_t startFrame = static_cast<size_t> (slice.start);
                const size_t stopFrame = static_cast<size_t> (slice.stop);

                return {
                    std::min (startFrame, numFrames),
                    std::min (stopFrame, numFrames)
                };
            }

            case Slice::Type::time:
            {
                const double sampleRateHz = getSampleRateHz();
                const size_t startFrame = roundToSizeT (slice.start * sampleRateHz);
                const size_t stopFrame = roundToSizeT (slice.stop * sampleRateHz);

                return {
                    std::min (startFrame, numFrames),
                    std::min (stopFrame, numFrames)
                };
            }

            default:
            {
                HART_THROW_OR_RETURN (hart::UnitError, "Slice type cannot be interpreted as frame range", std::make_pair (0, numFrames));
            }
        }
    }

    /// @brief Resamples this buffer to an arbitrary sample rate
    /// @details Under the hood, it uses
    /// [r8brain](https://github.com/avaneev/r8brain-free-src), which is said
    /// to be one of the most high-quality SRC libraries out there. 
    /// It is guaranteed that there's no latency introduced to the resampled signal.
    /// However, the exact waveform at boundary transients, i. e. at the very
    /// beginning and at the very end of the buffer, is not guaranteed to be preserved,
    /// due to SRC filtering.
    /// @param targetSampleRateHz Sample rate to resample to. Can be higher, lower,
    /// and can be any valid SR value, not just power-of-two ratios.
    /// @return A new buffer containing resampled audio
    AudioBuffer<SampleType> resample (double targetSampleRateHz) const
    {
        if (targetSampleRateHz < 0.0 || floatsEqual (targetSampleRateHz, 0.0) || std::isnan (targetSampleRateHz))
            HART_THROW_OR_RETURN (hart::SampleRateError, "Invalid target sample rate", AudioBuffer<SampleType> (m_numChannels, 0));

        if (! hasSampleRate())
            HART_THROW_OR_RETURN (hart::SampleRateError, "Can't resample, since this AudioBuffer doesn't have a sample rate value assigned to it", AudioBuffer<SampleType> (m_numChannels, 0, targetSampleRateHz));

        if (floatsEqual (targetSampleRateHz, m_sampleRateHz))
            return AudioBuffer<SampleType> (*this);  // Same sample rate - can get away with just a copy

        const double sourceLengthSeconds = getLengthSeconds();
        const size_t resampledBufferSizeFrames = roundToSizeT (sourceLengthSeconds * targetSampleRateHz);
        AudioBuffer<SampleType> resampledBuffer (m_numChannels, resampledBufferSizeFrames, targetSampleRateHz);
        hassert (floatsEqual (resampledBuffer.getLengthSeconds(), sourceLengthSeconds, 100e-6));  // It's okay to loosen this tolerance within reason, if it fails at some point

        for (size_t channel = 0; channel < m_numChannels; ++channel)
            hart::resample ((*this)[channel], m_numFrames, m_sampleRateHz, resampledBuffer[channel], resampledBufferSizeFrames, targetSampleRateHz);

        return resampledBuffer;
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

    /// @brief Prints readable text representation of the AudioBuffer object into the I/O stream
    /// @relates AudioBuffer
    friend std::ostream& operator<< (std::ostream& stream, const AudioBuffer& audioBuffer)
    {
        audioBuffer.represent (stream);
        return stream;
    }

    void _processWith (DSPBase<SampleType>& dsp, size_t blockSizeFrames, Preparation dspPreparation);
    void _fillWith (SignalBase<SampleType>& signal, size_t blockSizeFrames, Preparation signalPreparation);
};

}  // namespace hart

#include "hart_audio_buffer_fill_with.hpp"
#include "hart_audio_buffer_process_with.hpp"
