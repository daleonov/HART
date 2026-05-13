#pragma once

#include <complex>
#include <vector>
#include <cmath>
#include <sstream>

#include "hart_exceptions.hpp"
#include "metrics/hart_interpolated_peak_frequency.hpp"
#include "matchers/hart_matcher.hpp"
#include "hart_precision.hpp"
#include "hart_reducers.hpp"  // mean()
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

    bool match (AnalysisContext<SampleType> context) override
    {
        if (context.outputAudio().getNumFrames() < 64)
            HART_THROW_OR_RETURN (hart::SizeError, "Audio is too short for fundamental detection", false);

        ChannelFlags channelsToCheck = this->getChannelFlags();
        channelsToCheck.resize (context.outputAudio().getNumChannels());  // TODO: Do it in the parent class implementation

        if (! channelsToCheck.anyTrue())
            return true;  // Nothing to check

        // TODO: It's not the best fundamental frequency estimator. Implement something like McLeod at some point for it.
        m_observedHz = interpolatedPeakFrequency (context.outputSpectrum()).ch (channelsToCheck).get (mean());
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
