#pragma once

#include <vector>
#include <cmath>  // sin(), cos(), abs()
#include <complex>
#include <algorithm>
#include <utility>  // swap(), make_pair(), pair

#include "hart_exceptions.hpp"
#include "hart_utils.hpp"  // nan(), roundToSizeT()

namespace hart
{

/// @brief Frequency-domain representation of a multi-channel audio signal
/// @details
/// Stores complex spectra for each channel independently.
/// Spectrum is constructed by performing FFT on the entire signal,
/// zero-padding to the next power-of-two size if necessary.
///
/// Current implementation assumptions:
/// - Rectangular window (no windowing)
/// - Real-valued input signal
/// - One-sided spectrum only (`fftSize / 2 + 1` bins)
/// - Double floating-point precision, regardless of input signal value types
/// @ingroup DataStructures
class Spectrum
{
public:
    /// @brief Constructs spectrum from an audio buffer
    /// @param buffer Audio buffer to perform FFT on
    /// @throws hart::SampleRateError if the audio buffer has no sample rate metadata
    template <typename SampleType>
    Spectrum (const AudioBuffer<SampleType>& buffer)
    {
        if (! buffer.hasSampleRate())
            HART_THROW_OR_RETURN_VOID (hart::SampleRateError, "Audio buffer has no sample rate metadata");

        m_sampleRateHz = buffer.getSampleRateHz();
        m_numChannels = buffer.getNumChannels();
        m_data.resize (m_numChannels);

        const size_t numFrames = buffer.getNumFrames();

        m_fftSize = nextPowerOfTwo (std::max<size_t> (1, numFrames));
        m_numBins = m_fftSize / 2 + 1;

        for (size_t channel = 0; channel < m_numChannels; ++channel)
        {
            m_data[channel].resize (m_numBins);
            std::vector<std::complex<double>> fftBuffer (m_fftSize);
            const SampleType* samples = buffer[channel];

            for (size_t frame = 0; frame < numFrames; ++frame)
                fftBuffer[frame] = static_cast<double> (samples[frame]);

            for (size_t frame = numFrames; frame < m_fftSize; ++frame)
                fftBuffer[frame] = 0.0;

            performFFT (fftBuffer);

            for (size_t bin = 0; bin < m_numBins; ++bin)
                m_data[channel][bin] = fftBuffer[bin];
        }
    }

    /// @brief Returns number of channels
    size_t getNumChannels() const
    {
        return m_numChannels;
    }

    /// @brief Returns number of frequency bins per channel
    size_t getNumBins() const
    {
        return m_numBins;
    }

    /// @brief Returns FFT size
    size_t getFFTSize() const
    {
        return m_fftSize;
    }

    /// @brief Returns sample rate in Hz
    double getSampleRateHz() const
    {
        return m_sampleRateHz;
    }

    /// @brief Returns frequency corresponding to a bin index
    double getBinFrequencyHz (size_t binIndex) const
    {
        if (binIndex >= m_numBins)
            HART_THROW_OR_RETURN (hart::IndexError, "Bin index is out of range", hart::nan<double>());

        return static_cast<double> (binIndex) * m_sampleRateHz / m_fftSize;
    }

    double getBinWidthHz() const
    {
        return m_sampleRateHz / m_fftSize;
    }

    /// @brief Returns complex value of a frequency bin, by bin index
    std::complex<double> getBinValue (size_t channel, size_t binIndex) const
    {
        if (channel >= m_numChannels)
            HART_THROW_OR_RETURN (hart::IndexError, "Channel index is out of range", hart::nan<double>());

        if (binIndex >= m_numBins)
            HART_THROW_OR_RETURN (hart::IndexError, "Bin index is out of range", hart::nan<double>());

        return m_data[channel][binIndex];
    }

    /// @brief Returns complex value of a frequency bin, by frequency
    std::complex<double> getBinValue (size_t channel, double frequencyHz) const
    {
        return getBinValue (channel, findClosestBin (frequencyHz));
    }

    /// @brief Returns magnitude of a frequency bin, by bin index
    double getBinMagnitude (size_t channel, size_t binIndex) const
    {
        return std::abs (getBinValue (channel, binIndex));
    }

    /// @brief Returns magnitude of a frequency bin, by frequency
    double getBinMagnitude (size_t channel, double frequencyHz) const
    {
        return getBinMagnitude (channel, findClosestBin (frequencyHz));
    }

    /// @brief Returns pointer to magnitudes of a specific channel
    const std::complex<double>* operator[] (size_t channel) const
    {
        if (channel >= m_numChannels)
            HART_THROW_OR_RETURN (hart::IndexError, "Channel index is out of range", hart::nan<double>());

        return m_data[channel].data();
    }

    /// @brief Finds closest FFT bin to a given frequency
    size_t findClosestBin (double frequencyHz) const
    {
        if (frequencyHz < 0.0)
            HART_THROW_OR_RETURN (hart::ValueError, "This is one-sided spectrum, frequencyHz should be a positive value", 0);

        const size_t bin = roundToSizeT (frequencyHz * m_fftSize / m_sampleRateHz);
        return std::min (bin, m_numBins - 1);
    }

    /// @brief Returns a pair of indices representing a provided slice
    /// @param slice A Slice instance. Valid slice types are `Slice::Type::whole`
    /// `Slice::Type::bins` and `Slice::Type::freq`
    /// @retval first Index representing the beginning of the range, inclusive
    /// @retval second Index representing the end of the range, non-inclusive
    std::pair<size_t, size_t> getBinIndices (const Slice& slice) const
    {
        const size_t numBins = getNumBins();

        switch (slice.type)
        {
            case Slice::Type::whole:
            {
                return {0, numBins};
            }

            case Slice::Type::bins:
            {
                const size_t startBin = static_cast<size_t> (slice.start);
                const size_t stopBin = static_cast<size_t> (slice.stop);

                return {
                    std::min (startBin, numBins),
                    std::min (stopBin, numBins)
                };
            }

            case Slice::Type::frequency:
            {
                const size_t startBin = findClosestBin (slice.start);
                const size_t stopBin = findClosestBin (slice.stop);

                return {
                    std::min (startBin, numBins),
                    std::min (stopBin, numBins)
                };
            }

            default:
            {
                HART_THROW_OR_RETURN (hart::UnitError, "Slice type cannot be interpreted as bin range", std::make_pair (0, numBins));
            }
        }
    }

private:

    static size_t nextPowerOfTwo (size_t x)
    {
        size_t power = 1;

        while (power < x)
            power <<= 1;

        return power;
    }

    static void performFFT (std::vector<std::complex<double>>& data)
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
            const double angle = -hart::twoPi / static_cast<double> (len);
            const std::complex<double> wlen (std::cos (angle), std::sin (angle));

            for (size_t i = 0; i < n; i += len)
            {
                std::complex<double> w (1.0, 0.0);

                for (size_t j = 0; j < len / 2; ++j)
                {
                    const std::complex<double> u = data[i + j];
                    const std::complex<double> v = data[i + j + len / 2] * w;

                    data[i + j] = u + v;
                    data[i + j + len / 2] = u - v;

                    w *= wlen;
                }
            }
        }
    }

    double m_sampleRateHz = 0.0;

    size_t m_numChannels = 0;
    size_t m_fftSize = 0;
    size_t m_numBins = 0;

    std::vector<std::vector<std::complex<double>>> m_data;
};

} // namespace hart
