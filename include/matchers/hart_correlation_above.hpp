#pragma once

#include <cmath>  // round()
#include <iomanip>
#include <vector>
#include <sstream>

#include "hart_accurate_sum.hpp"
#include "hart_exceptions.hpp"
#include "hart_matcher.hpp"
#include "hart_precision.hpp"
#include "hart_utils.hpp"  // inf, floatsEqual()

namespace hart
{

/// @brief Checks whether the output signal is sufficiently correlated with the input signal
/// @details
/// Uses normalized cross-correlation in the time domain to compare input and output audio,
/// while searching for the best match within a configurable lag range.
/// Correlation is calculated independently for every applicable channel using the formula:
/// @f[
/// \frac{\sum_n x[n]\,y[n+k]}
///      {\sqrt{\left(\sum_n x[n]^2\right)\left(\sum_n y[n+k]^2\right)}}
/// @f]
///
/// (`sum (x[n] * y[n+k]) / sqrt (sum (x[n]^2) * sum (y[n+k]^2))`),
///
/// where `x` is input signal and `y` is observed output signal.
///
/// For multi-channel audio, the lowest correlation value across all applicable channels is used.
/// This matcher is useful for verifying transparent DSP, latency compensation, bypass paths,
/// or processors that preserve waveform shape while introducing delay or mild coloration.
/// Notes:
/// - Gain differences do not affect the result due to normalization.
/// - Constant DC offset reduces correlation.
/// - Heavy nonlinear processing may significantly reduce correlation.
/// - The absolute value of correlation is used, so polarity inversions do not affect the result.
/// @tparam SampleType Floating point sample type, typically `float` or `double`
/// @ingroup Matchers
template <typename SampleType>
class CorrelationAbove :
    public Matcher<SampleType, CorrelationAbove<SampleType>>
{
public:

    /// @brief Creates a correlation matcher with a minimum accepted correlation threshold
    /// @details The matcher scans lags in the range `[-maxLagSeconds, +maxLagSeconds]`
    /// and finds the best normalized cross-correlation value.
    /// A value of `1.0` requires a perfect waveform match (ignoring polarity and latency),
    /// while lower values allow progressively more waveform deviation.
    /// @param minCorrelation Minimum allowed absolute correlation in the range `[0, 1]`
    /// @param maxLagSeconds Maximum absolute lag to search in seconds
    CorrelationAbove (double minCorrelation, double maxLagSeconds = 0.01):
        m_minCorrelation (minCorrelation),
        m_maxLagSeconds (maxLagSeconds)
    {
        if (m_minCorrelation < 0 || m_minCorrelation > 1.0)
            HART_THROW_OR_RETURN (hart::ValueError, "Correlation should be in 0..1 range", false);

        if (m_maxLagSeconds < 0)
            HART_THROW_OR_RETURN (hart::ValueError, "Max lag should be a non-negative number in seconds", false);
    }

    void prepare (double sampleRateHz, size_t /* numChannels */, size_t /*maxBlockSizeFrames*/) override
    {
        m_sampleRateHz = sampleRateHz;
        m_maxLagFrames = static_cast<long long int> (std::round (m_maxLagSeconds * m_sampleRateHz));
    }

    bool canOperatePerBlock() const override
    {
        return false;
    }

    void reset() override
    {
        m_failureChannel = 0;
        m_failureFrame = 0;
        m_bestCorrelation = 0.0;
        m_bestLagFrames = 0;
        m_hadValidData = false;
    }

    bool match (const AudioBuffer<SampleType>& inputAudio, const AudioBuffer<SampleType>& observedOutputAudio) override
    {
        hassert (inputAudio.getNumChannels() == observedOutputAudio.getNumChannels());
        hassert (inputAudio.getNumFrames() == observedOutputAudio.getNumFrames());
        hassert (inputAudio.getSampleRateHz() == observedOutputAudio.getSampleRateHz());

        const size_t numChannels = inputAudio.getNumChannels();
        const long long int numFrames = static_cast<long long int> (inputAudio.getNumFrames());

        double worstChannelCorrelation = hart::inf;
        size_t worstChannel = 0;
        bool anyValidChannel = false;

        for (size_t channel = 0; channel < numChannels; ++channel)
        {
            if (! this->appliesToChannel (channel))
                continue;

            double bestCorrelation = -hart::inf;
            long long int bestLag = 0;
            bool channelValid = false;

            const SampleType* x = inputAudio[channel];
            const SampleType* y = observedOutputAudio[channel];

            // Formula:
            // sum (x[n] * y[n+k]) / sqrt (sum (x[n]^2) * sum (y[n+k]^2))

            for (long long int lag = -m_maxLagFrames; lag <= m_maxLagFrames; ++lag)
            {
                AccurateSum<SampleType> dotProduct = 0.0;
                AccurateSum<SampleType> sumSqX = 0.0;
                AccurateSum<SampleType> sumSqY = 0.0;
                size_t overlapCount = 0;

                for (long long int n = 0; n < numFrames; ++n)
                {
                    const long long int yn = n + lag;

                    if (yn < 0 || yn >= numFrames)
                        continue;

                    const SampleType xnVal = x[n];
                    const SampleType ynVal = y[yn];

                    dotProduct += xnVal * ynVal;
                    sumSqX += xnVal * xnVal;
                    sumSqY += ynVal * ynVal;

                    ++overlapCount;
                }

                if (overlapCount == 0 || floatsEqual<SampleType> (sumSqX, 0) || floatsEqual<SampleType> (sumSqY, 0))
                    continue;

                channelValid = true;
                const double corr = static_cast<double> (dotProduct) / std::sqrt (static_cast<double> (sumSqX * sumSqY));
                const double absCorr = std::abs (corr);

                if (absCorr > bestCorrelation)
                {
                    bestCorrelation = absCorr;
                    bestLag = lag;
                }

                if (floatsEqual (absCorr, 1.0))
                {
                    bestCorrelation = absCorr;
                    bestLag = lag;
                    break;
                }
            }

            if (! channelValid)
                continue;

            anyValidChannel = true;

            if (bestCorrelation < worstChannelCorrelation)
            {
                worstChannelCorrelation = bestCorrelation;
                worstChannel = channel;
                m_bestCorrelation = bestCorrelation;
                m_bestLagFrames = bestLag;
            }
        }

        if (! anyValidChannel)
        {
            m_hadValidData = false;
            m_failureChannel = 0;
            m_failureFrame = 0;
            return false;
        }

        m_hadValidData = true;

        if (worstChannelCorrelation >= m_minCorrelation)
            return true;

        m_failureChannel = worstChannel;
        m_failureFrame = 0; // no specific frame failure

        return false;
    }

    MatcherFailureDetails getFailureDetails() const override
    {
        MatcherFailureDetails details;
        details.channel = m_failureChannel;
        details.frame = m_failureFrame;

        if (! m_hadValidData)
        {
            details.description = "Correlation could not be computed (no valid signal overlap)";
            return details;
        }

        const double lagSeconds = m_bestLagFrames / m_sampleRateHz;
        std::stringstream stream;

        stream
            << "Best correlation: " << correlationPrecision << m_bestCorrelation
            << " at lag " << m_bestLagFrames << " frames ("
            << secPrecision << lagSeconds << " seconds)";

        details.description = stream.str();
        return details;
    }

    void represent (std::ostream& stream) const override
    {
        stream
            << "CorrelationAbove ("
            << correlationPrecision << m_minCorrelation << ", "
            << secPrecision << m_maxLagSeconds << "_s)";
    }

private:
    const double m_minCorrelation;
    const double m_maxLagSeconds;

    double m_sampleRateHz = 0.0;
    long long int m_maxLagFrames = 0;

    double m_bestCorrelation = 0.0;
    long long m_bestLagFrames = 0;

    size_t m_failureChannel = 0;
    size_t m_failureFrame = 0;
    bool m_hadValidData = false;
};

HART_MATCHER_DECLARE_ALIASES_FOR (CorrelationAbove)

} // namespace hart
