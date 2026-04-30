#pragma once

#include <complex>
#include <vector>
#include <cmath>
#include <sstream>

#include "hart_exceptions.hpp"
#include "matchers/hart_matcher.hpp"
#include "hart_precision.hpp"
#include "hart_utils.hpp"

namespace hart
{

/// @brief Checks the fundamental frequency of the signal
/// @details Uses full-buffer zero-padded FFT + parabolic interpolation on the strongest bin.
/// Works correctly on anything with a strong fundamental.
/// If multiple channels are enabled for this matcher, it will check the mono sum of the signal.
/// If you require every channel to match this frequency, do multiple per-channel assertions,
/// using @ref hart::Matcher::atChannel().
/// @ingroup Matchers
template<typename SampleType>
class FundamentalEquals :
    public Matcher<SampleType, FundamentalEquals<SampleType>>
{
public:
    /// @brief Creates a matcher for a specific fundamental frequency
    /// @param expectedFundamentalHz Expected fundamental frequency in Hz
    /// @param toleranceCents Tolerance in cents
    FundamentalEquals (double expectedFundamentalHz, double toleranceCents = 1.0) :
        m_expectedFundamentalHz (expectedFundamentalHz),
        m_toleranceCents (toleranceCents)
    {
        if (expectedFundamentalHz <= 0.0)
            HART_THROW (hart::ValueError, "Target frequency must be > 0");
    }

    void prepare (double sampleRateHz, size_t /* numInputChannels */, size_t /* numOutputChannels */, size_t /* maxBlockSizeFrames */) override
    {
        m_sampleRateHz = sampleRateHz;
    }

    bool match (const AudioBuffer<SampleType>& /* inputAudio */, const AudioBuffer<SampleType>& observedOutputAudio) override
    {
        const size_t numFrames = observedOutputAudio.getNumFrames();

        if (numFrames < 64)
            HART_THROW_OR_RETURN (hart::SizeError, "Audio is too short for fundamental detection", false);

        if (! this->m_channelsToMatch.anyTrue())
            return true;  // Nothing to check

        // If multiple channels are to be checked, sum them to mono
        // Switching to double here, to make things more simple (and precise)
        std::vector<double> observedAudioMono (numFrames, 0.0);
        const double numChannelsSelected = static_cast<double> (this->m_channelsToMatch.numTrue());

        for (size_t channel = 0; channel < observedOutputAudio.getNumChannels(); ++channel)
        {
            if (! this->appliesToChannel (channel))
                continue;

            for (size_t frame = 0; frame < numFrames; ++frame)
                observedAudioMono[frame] += static_cast<double> (observedOutputAudio[channel][frame]) / numChannelsSelected;
        }

        // Next power of 2 after numFrames
        size_t fftSize = 1;

        while (fftSize < numFrames)
            fftSize <<= 1;

        // Zero-pad and FFT
        std::vector<std::complex<double>> spectrum (fftSize, 0.0);

        for (size_t i = 0; i < numFrames; ++i) 
            spectrum[i] = observedAudioMono[i];

        calculateFFTInPlace (spectrum);

        // Find strongest bin (skip DC and Nyquist frequency)
        double maxPower = -1.0;
        size_t strongestBin = 0;

        for (size_t bin = 2; bin < fftSize / 2; ++bin)
        {
            const double power = std::norm (spectrum[bin]);

            if (power > maxPower)
            {
                maxPower = power;
                strongestBin  = bin;
            }
        }

        // Parabolic interpolation on magnitude
        const double ym1 = std::abs (spectrum[strongestBin - 1]);
        const double y0  = std::abs (spectrum[strongestBin]);
        const double yp1 = std::abs (spectrum[strongestBin + 1]);

        const double delta = 0.5 * (ym1 - yp1) / (ym1 - 2.0 * y0 + yp1 + 1e-30);
        const double preciseBin = static_cast<double> (strongestBin) + delta;

        m_observedHz = preciseBin * m_sampleRateHz / static_cast<double> (fftSize);
        const double deviationCents = 1200.0 * std::log2 (m_observedHz / m_expectedFundamentalHz);

        if (std::abs (deviationCents) > m_toleranceCents)
        {
            m_centsError = deviationCents;
            return false;
        }

        return true;
    }

    bool canOperatePerBlock() const override
    {
        return false;
    }

    void reset() override {}

    MatcherFailureDetails getFailureDetails() const override
    {
        std::stringstream stream;
        stream << "Observed fundamental "
            << hzPrecision << m_observedHz << " Hz ("
            << centsPrecision << m_centsError << " cents off)";

        MatcherFailureDetails details;
        details.frame = 0;  // Actually, more like a whole buffer is off
        details.channel = 0;  // All the channels, actually
        details.description = stream.str();
        return details;
    }

    void represent (std::ostream& s) const override
    {
        s << "FundamentalEquals ("
            << hzPrecision << m_expectedFundamentalHz << "_Hz, "
            << centsPrecision << m_toleranceCents << "_cents)";
    }

private:
    const double m_expectedFundamentalHz;
    const double m_toleranceCents;
    double m_sampleRateHz = 44100.0;
    double m_observedHz = 0.0;
    double m_centsError = 0.0;

    static void calculateFFTInPlace (std::vector<std::complex<double>>& spectrum)
    {
        // Bit reversal
        size_t log2n = 0;

        for (size_t temp = spectrum.size(); temp > 1; temp >>= 1)
            ++log2n;

        for (size_t i = 0; i < spectrum.size(); ++i)
        {
            size_t rev = 0;

            for (size_t j = 0; j < log2n; ++j)
                if (i & (size_t (1) << j))
                    rev |= size_t (1) << (log2n - 1 - j);

            if (i < rev)
                std::swap (spectrum[i], spectrum[rev]);
        }

        // Butterflies
        for (size_t len = 2; len <= spectrum.size(); len <<= 1)
        {
            const double angleRadians = -hart::twoPi / len;
            const std::complex<double> wlen (std::cos (angleRadians), std::sin (angleRadians));

            for (size_t i = 0; i < spectrum.size(); i += len)
            {
                std::complex<double> w (1.0);
                for (size_t j = 0; j < len / 2; ++j)
                {
                    std::complex<double> u = spectrum[i + j];
                    std::complex<double> v = spectrum[i + j + len / 2] * w;
                    spectrum[i + j] = u + v;
                    spectrum[i + j + len / 2] = u - v;
                    w *= wlen;
                }
            }
        }
    }
};

HART_MATCHER_DECLARE_ALIASES_FOR (FundamentalEquals)

}  // namespace hart
