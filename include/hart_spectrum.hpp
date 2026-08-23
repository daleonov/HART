#pragma once

#include <cstdint>  // uint_fast32_t
#include <vector>
#include <cmath>  // sin(), cos(), abs()
#include <complex>  // complex, conj(), real(), imag()
#include <algorithm>
#include <random>
#include <utility>  // swap(), make_pair(), pair

#include "hart_audio_buffer.hpp"
#include "hart_cliconfig.hpp"
#include "hart_exceptions.hpp"
#include "hart_slice.hpp" 
#include "signals/hart_audio_buffer_signal.hpp"  // for toSignal()
#include "hart_utils.hpp"  // nan(), roundToSizeT(), nextPowerOfTwo()

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
        m_sizeInTimeDomainFrames = numFrames;

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

            performFFT (fftBuffer, false);

            for (size_t bin = 0; bin < m_numBins; ++bin)
                m_data[channel][bin] = fftBuffer[bin];
        }
    }

    static Spectrum zeros (size_t numChannels = CLIConfig::getInstance().getDefaultNumInputChannels(), size_t signalDurationFrames = CLIConfig::getInstance().getDefaultRenderDurationFrames(), double sampleRateHz = CLIConfig::getInstance().getDefaultSampleRateHz())
    {
        Spectrum spectrum;

        if (sampleRateHz < 0)
            HART_THROW_OR_RETURN_VOID (hart::SampleRateError, "Sample rate should not be negative");

        spectrum.m_sampleRateHz = sampleRateHz;

        // We can allow zero channels or zero frames at some point, but need to add
        // guards to all methods that might get zero division errors as a result

        if (numChannels == 0)
            HART_THROW_OR_RETURN_VOID (hart::SizeError, "Number of channels should not be zero");

        if (signalDurationFrames == 0)
            HART_THROW_OR_RETURN_VOID (hart::SizeError, "Signal duration should not be zero");

        spectrum.m_fftSize = nextPowerOfTwo (signalDurationFrames);
        spectrum.m_numBins = spectrum.m_fftSize / 2 + 1;
        spectrum.m_sizeInTimeDomainFrames = signalDurationFrames;

        spectrum.m_numChannels = numChannels;
        spectrum.m_data.resize (
            spectrum.m_numChannels,
            std::vector<std::complex<double>> (spectrum.m_numBins, std::complex<double> (0.0, 0.0))
            );

        return spectrum;
    }

    /// @brief Common values for `beta` argument in the `colouredNoise()` factory
    enum BetaFor
    {
        whiteNoise = 0,  ///< `beta` value for white noise, for the `colouredNoise()` factory
        pinkNoise = -1,  ///< `beta` value for pink noise, for the `colouredNoise()` factory
        brownNoise = -2,  ///< `beta` value for brown (a.k.a. red, or Brownian) noise, for the `colouredNoise()` factory
        blueNoise = 1,  ///< `beta` value for blue noise, for the `colouredNoise()` factory
        violetNoise = 2  ///< `beta` value for violet noise, for the `colouredNoise()` factory
    };

    /// @brief Options for `colouredNoise()` method
    class ColouredNoise
    {
        public:
            /// @brief Creates ColouredNoise with a custom `beta` value
            /// @details For common noise types, use provided factory methods:
            ///
            /// Noise type          | Method     | `beta`
            /// --------------------|------------|-------
            /// White               | `white()`  |  0
            /// Pink                | `pink()`   | −1
            /// Brown/Red/Brownian  | `brown()`  | −2
            /// Blue                | `blue()`   | +1
            /// Violet              | `violet()` | +2
            ///
            /// @param beta Power-spectrum exponent (slope):
            /// @return Chainable `ColouredNoise` options instance
            explicit ColouredNoise (float beta)
            {
                m_beta = beta;
            }

            /// @brief Creates ColouredNoise instance with `beta` value for white noise
            /// @return Chainable `ColouredNoise` options instance
            static ColouredNoise white()
            {
                return ColouredNoise (0.0);
            }

            /// @brief Creates ColouredNoise instance with `beta` value for pink noise
            /// @return Chainable `ColouredNoise` options instance
            static ColouredNoise pink()
            {
                return ColouredNoise (-1.0);
            }

            /// @brief Creates ColouredNoise instance with `beta` value for brown (a.k.a. red, or Brownian) noise
            /// @return Chainable `ColouredNoise` options instance
            static ColouredNoise brown()
            {
                return ColouredNoise (-2.0);
            }

            /// @brief Creates ColouredNoise instance with `beta` value for blue noise
            /// @return Chainable `ColouredNoise` options instance
            static ColouredNoise blue()
            {
                return ColouredNoise (1.0);
            }

            /// @brief Creates ColouredNoise instance with `beta` value for violet noise
            /// @return Chainable `ColouredNoise` options instance
            static ColouredNoise violet()
            {
                return ColouredNoise (2.0);
            }

            /// @brief Sets random seed
            /// @param randomSeed RNG seed. While magnitudes will have ideal
            /// values, the real and imaginary parts of bins will be randomized.
            ///
            /// Defaults to global default random seed.
            /// @return Chainable `ColouredNoise` options instance
            ColouredNoise withRandomSeed (uint_fast32_t randomSeed)
            {
                m_randomSeed = randomSeed;
                return *this;
            }

            /// @brief Sets the lowest meaningful frequency in the spectrum
            /// @param lowCutoffFrequencyHz The starting frequency of the noise.
            /// Defaults to 20 Hz.
            /// @note Bins below it will be zero. Must be at least 1 Hz.
            /// @return Chainable `ColouredNoise` options instance
            ColouredNoise withLowCutoff (double lowCutoffFrequencyHz)
            {
                if (lowCutoffFrequencyHz < 1.0)
                    HART_THROW_OR_RETURN (hart::ValueError, "Low cutoff frequency should be at least 1 Hz", *this);

                m_lowCutoffFrequencyHz = lowCutoffFrequencyHz;
                return *this;
            }

            /// @brief Sets number of channels
            /// @note Each channel will contain a difference noise instance
            /// @param numChannels Number of channels in the generated spectrum
            ///
            /// Defaults to global default number of input channels.
            ColouredNoise withNumChannels (size_t numChannels)
            {
                m_numChannels = numChannels;
                return *this;
            }

            /// @brief Sets duration of time-domain audio that spectrum will represent
            /// @param signalDurationFrames Desired duration of the signal this
            /// noise represents. Can be arbitrary. If not a power of 2, the
            /// spectrum will be padded to the next power of 2, but if converted
            /// to time domain via `toAudioBuffer()`, the desired duration will
            /// be respected.
            ///
            /// Defaults to global default render duration.
            /// @return Chainable `ColouredNoise` options instance
            ColouredNoise withDuration (size_t signalDurationFrames)
            {
                m_signalDurationFrames = signalDurationFrames;
                return *this;
            }

            /// @brief Sets sample rate
            /// @param sampleRateHz Sample rate of the generated spectrum in Hertz.
            ///
            /// Defaults to global default sample rate.
            /// @return Chainable `ColouredNoise` options instance
            ColouredNoise withSampleRate (double sampleRateHz)
            {
                if (sampleRateHz < 0.0 || floatsEqual (sampleRateHz, 0.0) || std::isnan (sampleRateHz))
                    HART_THROW_OR_RETURN (hart::SampleRateError, "Invalid sample rate value", *this);

                m_sampleRateHz = sampleRateHz;
                return *this;
            }

            ///@private
            double getBeta() { return m_beta; }

            ///@private
            double getLowCutoffHz() { return m_lowCutoffFrequencyHz; }

            ///@private
            double getSampleRateHz() { return m_sampleRateHz; }

            ///@private
            uint_fast32_t getRandomSeed() { return m_randomSeed; }

            ///@private
            size_t getNumChannels() { return m_numChannels; }

            ///@private
            size_t getDurationFrames() { return m_signalDurationFrames; }

        private:
            double m_beta = 0.0;
            double m_lowCutoffFrequencyHz = 20.0;
            double m_sampleRateHz = CLIConfig::getInstance().getDefaultSampleRateHz();
            uint_fast32_t m_randomSeed = CLIConfig::getInstance().getRandomSeed();
            size_t m_numChannels = CLIConfig::getInstance().getDefaultNumInputChannels();
            size_t m_signalDurationFrames = CLIConfig::getInstance().getDefaultRenderDurationFrames();

            ColouredNoise() = default;  // Beta must always be explicitly defined
    };

    /// @brief Creates deterministic coloured noise in the frequency domain
    /// @details `beta` is interpreted as a power-spectrum exponent, so the
    /// stored bin magnitudes follow:
    /// @f[
    /// |X(f)| \propto {f_{rel}}^ \frac{\beta}{2}
    /// @f]
    /// (|X(f)| ~ f_rel ** (beta / 2)),
    ///
    /// where f_rel = frequency / lowCutoffHz.
    /// @param options `ColouredNoise` containing 
    static Spectrum colouredNoise (ColouredNoise options)
    {
        Spectrum spectrum = Spectrum::zeros (options.getNumChannels(), options.getDurationFrames(), options.getSampleRateHz());
        std::mt19937 randomNumberGenerator (options.getRandomSeed());
        std::uniform_real_distribution<double> phaseDistribution (0.0, hart::twoPi);
        std::uniform_int_distribution<int> signDistribution (0, 1);

        const double lowCutoffHz = options.getLowCutoffHz();
        const double beta = options.getBeta();
        const double magnitudeExponent = beta / 2.0;

        for (size_t channel = 0; channel < spectrum.m_numChannels; ++channel)
        {
            for (size_t bin = 0; bin < spectrum.m_numBins; ++bin)
            {
                const double frequencyHz = spectrum.getBinFrequencyHz (bin);

                if (frequencyHz < lowCutoffHz || (frequencyHz == 0.0 && beta < 0.0))
                {
                    spectrum.m_data[channel][bin] = std::complex<double> (0.0, 0.0);
                    continue;
                }

                const double magnitude = std::pow (frequencyHz / lowCutoffHz, magnitudeExponent);

                if (bin == 0 || (spectrum.m_fftSize % 2 == 0 && bin == spectrum.m_numBins - 1))
                {
                    const double signedMagnitude = signDistribution (randomNumberGenerator) == 0 ? -magnitude : magnitude;
                    spectrum.m_data[channel][bin] = std::complex<double> (signedMagnitude, 0.0);
                    continue;
                }

                const double phase = phaseDistribution (randomNumberGenerator);
                spectrum.m_data[channel][bin] = std::complex<double> (magnitude * std::cos (phase), magnitude * std::sin (phase));
            }
        }

        return spectrum;
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

    /// @brief Returns original audio buffer duration in frames
    size_t getSizeInTimeDomainFrames() const
    {
        return m_sizeInTimeDomainFrames;
    }

    /// @brief Returns sample rate in Hz
    double getSampleRateHz() const
    {
        return m_sampleRateHz;
    }

    /// @brief Converts this spectrum to a time-domain audio buffer
    /// @details
    /// Reconstructs the omitted negative-frequency bins from Hermitian symmetry,
    /// performs an inverse FFT, and trims the result to the original signal length.
    /// @tparam SampleType Type of samples for the resulting buffer, typically
    /// `float` or `double`
    template <typename SampleType>
    AudioBuffer<SampleType> toAudioBuffer() const
    {
        AudioBuffer<SampleType> buffer (m_numChannels, m_sizeInTimeDomainFrames, m_sampleRateHz);

        for (size_t channel = 0; channel < m_numChannels; ++channel)
        {
            std::vector<std::complex<double>> fftBuffer (m_fftSize, std::complex<double> (0.0, 0.0));

            for (size_t bin = 0; bin < m_numBins; ++bin)
                fftBuffer[bin] = m_data[channel][bin];

            for (size_t bin = 1; bin + 1 < m_numBins; ++bin)
                fftBuffer[m_fftSize - bin] = std::conj (m_data[channel][bin]);

            performFFT (fftBuffer, true);
            SampleType* channelData = buffer[channel];

            for (size_t frame = 0; frame < m_sizeInTimeDomainFrames; ++frame)
                channelData[frame] = static_cast<SampleType> (fftBuffer[frame].real());
        }

        return buffer;
    }

    /// @brief Converts the audio represented by this Spectrum to a Signal source
    /// @param loop Whether the signal should loop after the data represented by
    /// this Spectrum's implied diration runs out
    /// @tparam SampleType Type of sample sata generated by the signal, typically
    /// `float` or `double`
    /// @return `AudioBufferSignal` instance, playing the signal with the spectrum
    /// identical to this one
    template <typename SampleType>
    AudioBufferSignal<SampleType> toSignal (Loop loop = Loop::no) const
    {
        return toAudioBuffer<SampleType>().toSignal (loop);
    }

    /// @brief Checks whether this spectrum contains approximately the same data as another spectrum
    /// @details The comparison is relaxed - each pair of corresponding complex bins must differ by
    /// no more than `toleranceLinear`. Both spectra must have the same dimensions, FFT size,
    /// time-domain size, and sample rate.
    /// @param other Spectrum to compare against
    /// @param toleranceLinear Absolute complex-bin tolerance in linear domain,
    /// for both real and imaginary parts.
    /// @return `true` if the spectra are equal within the specified tolerance, `false` otherwise
    bool equalsTo (const Spectrum& other, double toleranceLinear = 1e-6) const
    {
        if (m_numChannels != other.m_numChannels
            || m_fftSize != other.m_fftSize
            || m_numBins != other.m_numBins
            || m_sizeInTimeDomainFrames != other.m_sizeInTimeDomainFrames)
            return false;

        if (! floatsEqual (m_sampleRateHz, other.m_sampleRateHz))
            return false;

        for (size_t channel = 0; channel < m_numChannels; ++channel)
        {
            for (size_t bin = 0; bin < m_numBins; ++bin)
            {
                const std::complex<double> thisValue = m_data[channel][bin];
                const std::complex<double> otherValue = other.m_data[channel][bin];

                if (std::abs (std::real (thisValue - otherValue)) > toleranceLinear || std::abs (std::imag (thisValue - otherValue)) > toleranceLinear)
                    return false;
            }
        }

        return true;
    }

    /// @brief Checks whether two spectra contain approximately the same data
    /// @details Equivalent to calling `Spectrum::equalsTo (other)` with the default tolerance
    /// @param other Spectrum to compare against
    /// @return `true` if the spectra are equal within the default tolerance, `false` otherwise
    bool operator== (const Spectrum& other) const
    {
        return equalsTo (other);
    }

    /// @brief Checks whether two spectra differ beyond the default comparison tolerance
    /// @details Equivalent to `! Spectrum::equalsTo (other)`
    /// @param other Spectrum to compare against
    /// @return `true` if the spectra are not equal within the default tolerance, `false` otherwise
    bool operator!= (const Spectrum& other) const
    {
        return ! equalsTo (other);
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

    /// @brief Returns pointer to read-only magnitudes of a specific channel
    const std::complex<double>* operator[] (size_t channel) const
    {
        if (channel >= m_numChannels)
            HART_THROW_OR_RETURN (hart::IndexError, "Channel index is out of range", hart::nan<double>());

        return m_data[channel].data();
    }

    /// @brief Returns pointer to mutable magnitudes of a specific channel
    std::complex<double>* operator[] (size_t channel)
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

    /// @brief Prints readable representation of the spectrum object
    /// @param stream String stream to append the representation to
    void represent (std::ostream& stream) const
    {
        stream << "<Spectrum>";
    }

    /// @brief Prints readable text representation of the spectrum object into the I/O stream
    /// @relates Spectrum
    friend std::ostream& operator<< (std::ostream& stream, const Spectrum& spectrum)
    {
        spectrum.represent (stream);
        return stream;
    }

private:
    Spectrum() = default;

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

    double m_sampleRateHz = 0.0;

    size_t m_numChannels = 0;
    size_t m_fftSize = 0;
    size_t m_numBins = 0;
    size_t m_sizeInTimeDomainFrames = 0;

    std::vector<std::vector<std::complex<double>>> m_data;
};

} // namespace hart
