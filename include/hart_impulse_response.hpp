#pragma once

#include <algorithm>  // max(), swap()
#include <cmath>  // isnan(), isinf()
#include <complex>
#include <iostream>
#include <vector>
#include <sstream>

#include "hart_audio_buffer.hpp"
#include "hart_exceptions.hpp"
#include "hart_utils.hpp"  // floatsEqual(), nextPowerOfTwo()

namespace hart
{

/// @brief Container for representing an impulse response (IR)
/// @details It is guaranteed to have a specific sample rate associated with it
/// @ingroup DataStructures
template <typename SampleType>
class ImpulseResponse
{
public:
    // TODO: Add a ctor that takes a pair of hart::Spectrum instances

    /// @brief Create an IR instance from a pait of time-domain signals
    /// @param input Signal at the input of the system
    /// @param output Signal at the output of the system
    /// @throws hart::SampleRateError if provided audio buffers either
    /// don't have sample rate, or it has invalid value
    /// @throws hart::SizeError if the lengths of provided buffers
    /// (in frames) don't match
    /// @throws hart::ChannelLayoutError if numbers of channels of the
    /// provided buffers don't match
    ImpulseResponse (AudioBuffer<SampleType> input, AudioBuffer<SampleType> output)
    {
        if (! input.hasSampleRate())
            HART_THROW (hart::SampleRateError, "Input buffer should have sample rate associated with it");
            
        if (! output.hasSampleRate())
            HART_THROW (hart::SampleRateError, "Output buffer should have sample rate associated with it");

        const double inputSampleRateHz = input.getSampleRateHz();
        const double outputSampleRateHz = output.getSampleRateHz();

        if (hart::floatsEqual (inputSampleRateHz, 0.0)
            || inputSampleRateHz < 0.0
            || std::isnan (inputSampleRateHz)
            || std::isinf (inputSampleRateHz)
        )
        {
            HART_THROW (hart::SampleRateError, "Input buffer should have valid sample rate");
        }
        
        if (hart::floatsEqual (outputSampleRateHz, 0.0)
            || outputSampleRateHz < 0.0
            || std::isnan (outputSampleRateHz)
            || std::isinf (outputSampleRateHz)
        )
        {
            HART_THROW (hart::SampleRateError, "Output buffer should have valid sample rate");
        }

        if (! hart::floatsEqual (inputSampleRateHz, outputSampleRateHz))
            HART_THROW (hart::SampleRateError, "Input and output buffers should have matching sample rates");

        if (input.getNumFrames() != output.getNumFrames())
            HART_THROW (hart::SizeError, "Input and output buffers should have matching number of frames");

        if (input.getNumChannels() != output.getNumChannels())
            HART_THROW (hart::ChannelLayoutError, "Input and output buffers should have matching number of channels");

        m_sampleRateHz = inputSampleRateHz;

        std::ostringstream ctorArgumentsRepresentationStream;
        ctorArgumentsRepresentationStream << input << ", " << output;
        m_ctorArgumentsRepresentation = ctorArgumentsRepresentationStream.str();

        m_numChannels = input.getNumChannels();
        m_numFrames = input.getNumFrames();
        m_frames.resize (m_numChannels * m_numFrames);
        m_channelPointers.resize (m_numChannels);
        updateChannelPointers();

        calculateImpulseResponse (input, output);
    }

    /// @brief Get number of channels
    /// @return Number of allocated channels
    size_t getNumChannels() const { return m_numChannels; }

    /// @brief Get number of frames (samples)
    /// @return Number of allocated frames (samples) in every channel
    size_t getNumFrames() const { return m_numFrames; }

    /// @brief Get a sample rate metadata
    /// @return IR's sample rate in Hz
    double getSampleRateHz() const
    {
        // IR should have a valid sample rate associated with it
        hassert (! std::isnan (m_sampleRateHz));
        hassert (! std::isinf (m_sampleRateHz));
        hassert (m_sampleRateHz > (SampleType) 0);

        return m_sampleRateHz;
    }

    /// @brief Prints readable representation of the IR
    /// @param stream String stream to append the representation to
    void represent (std::ostream& stream) const
    {
        stream << "ImpulseResponse (" << m_ctorArgumentsRepresentation << ")";
    }

    /// @brief Get a raw pointer to a specific channel's read-only audio data
    /// @note The data is guaranteed to have at least `getNumFrames()` items and to be a contiguous non-interleaved block of memory.
    /// @return Pointer to the IR data of requested channel
    const SampleType* operator[] (size_t channel) const
    {
        return m_channelPointers[channel];
    }

    /// @brief Prints readable text representation of the ImpulseResponse object into the I/O stream
    /// @relates ImpulseResponse
    friend std::ostream& operator<< (std::ostream& stream, const ImpulseResponse& ir)
    {
        ir.represent (stream);
        return stream;
    }

private:
    std::string m_ctorArgumentsRepresentation;
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

    void calculateImpulseResponse (const AudioBuffer<SampleType>& input, const AudioBuffer<SampleType>& output)
    {
        const size_t fftSize = nextPowerOfTwo (std::max<size_t> (1, m_numFrames));

        for (size_t channel = 0; channel < m_numChannels; ++channel)
        {
            std::vector<std::complex<double>> inputSpectrum (fftSize);
            std::vector<std::complex<double>> outputSpectrum (fftSize);

            for (size_t frame = 0; frame < m_numFrames; ++frame)
            {
                inputSpectrum[frame] = static_cast<double> (input[channel][frame]);
                outputSpectrum[frame] = static_cast<double> (output[channel][frame]);
            }

            performFFT (inputSpectrum, false);
            performFFT (outputSpectrum, false);

            for (size_t bin = 0; bin < fftSize; ++bin)
            {
                if (hart::floatsEqual (std::norm (inputSpectrum[bin]), 0.0))
                    HART_THROW_OR_RETURN_VOID (hart::ValueError, "Cannot calculate impulse response from an input buffer with empty frequency bins");

                outputSpectrum[bin] /= inputSpectrum[bin];
            }

            performFFT (outputSpectrum, true);

            SampleType* irChannel = m_channelPointers[channel];

            for (size_t frame = 0; frame < m_numFrames; ++frame)
                irChannel[frame] = static_cast<SampleType> (outputSpectrum[frame].real());
        }
    }

    static void performFFT (std::vector<std::complex<double>>& data, bool isInverse)
    {
        const size_t n = data.size();

        hassert (n != 0);
        hassert ((n & (n - 1)) == 0);  // Size should be a power of 2

        size_t j = 0;

        for (size_t i = 1; i < n; ++i)
        {
            size_t bit = n >> 1;

            while (j & bit)
            {
                j ^= bit;
                bit >>= 1;
            }

            j ^= bit;

            if (i < j)
                std::swap (data[i], data[j]);
        }

        for (size_t len = 2; len <= n; len <<= 1)
        {
            const double angle = (isInverse ? hart::twoPi : -hart::twoPi) / static_cast<double> (len);
            const std::complex<double> wlen (std::cos (angle), std::sin (angle));

            for (size_t i = 0; i < n; i += len)
            {
                std::complex<double> w (1.0, 0.0);

                for (size_t jj = 0; jj < len / 2; ++jj)
                {
                    const std::complex<double> u = data[i + jj];
                    const std::complex<double> v = data[i + jj + len / 2] * w;

                    data[i + jj] = u + v;
                    data[i + jj + len / 2] = u - v;

                    w *= wlen;
                }
            }
        }

        if (isInverse)
        {
            const double scale = 1.0 / static_cast<double> (n);

            for (size_t i = 0; i < n; ++i)
                data[i] *= scale;
        }
    }
};

}  // namespace hart
