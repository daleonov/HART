#pragma once

#include <cmath>
#include <iomanip>
#include <sstream>
#include <vector>

#include "hart_accurate_sum.hpp"
#include "hart_exceptions.hpp"
#include "hart_matcher.hpp"
#include "hart_precision.hpp"
#include "hart_silence_policy.hpp"
#include "hart_utils.hpp"

namespace hart
{

/// @brief Checks whether the output signal preserves the polarity of the input signal
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
/// The lag with the highest absolute correlation is used to compensate for latency,
/// and the signed correlation at this lag is then checked against a minimum signed
/// correlation threshold.
///
/// For multi-channel audio, all applicable channels must preserve polarity.
/// If at least one applicable channel exceeds the negative threshold, the match fails.
///
/// This matcher is useful for detecting accidental polarity inversions while remaining
/// robust to latency and gain differences.
///
/// Notes:
/// - Gain differences do not affect the result due to normalization.
/// - Constant DC offset may bias the signed correlation.
/// - Heavy nonlinear processing may reduce confidence in polarity detection.
/// - Unlike @ref hart::CorrelationAbove, the sign of correlation is preserved.
/// @tparam SampleType Floating point sample type, typically `float` or `double`
/// @ingroup Matchers
template <typename SampleType>
class PolarityPreserved :
    public Matcher<SampleType, PolarityPreserved<SampleType>>
{
public:

    /// @brief Creates a polarity matcher with a minimum signed correlation threshold
    /// @details
    /// The matcher scans lags in the range `[-maxLagSeconds, +maxLagSeconds]`
    /// and finds the lag with the strongest absolute normalized cross-correlation.
    ///
    /// Polarity is considered preserved only if the signed correlation at the best lag
    /// is greater than or equal to `minimumSignedCorrelation`.
    ///
    /// Lower values make polarity detection more tolerant to distortion, noise,
    /// or other waveform changes, while higher values require a cleaner match.
    ///
    /// @param minimumSignedCorrelation Minimum required correlation between input
    /// and the output signal. If the observed correlation is in
    /// `(minimumSignedCorrelation, minimumSignedCorrelation)` range, the matcher will
    /// fail due to the signals being weakly correlated. If correlation is in
    /// [-1, -minimumSignedCorrelation] range, the phase is considered flipped.
    /// If it falls into [minimumSignedCorrelation, 1] range, it is considered
    /// preserved, and this is where the matcher will pass.
    /// @param maxLagSeconds Maximum absolute lag to search in seconds
    /// @param silencePolicy Defines how channels with silence (zeros or almost zeros)
    /// are handled. Available options are:
    ///   - `SilencePolicy::strict` - fails if any applicable channel is silent
    ///   - `SilencePolicy::relaxed` - ignores silent channels, as long as at least one
    ///     channel is not silent
    /// @see hart::SilencePolicy
    PolarityPreserved (double minimumSignedCorrelation = 0.5, double maxLagSeconds = 0.01, SilencePolicy silencePolicy = SilencePolicy::strict):
        m_minimumSignedCorrelation (minimumSignedCorrelation),
        m_maxLagSeconds (maxLagSeconds),
        m_silencePolicy (silencePolicy)
    {
        if (m_minimumSignedCorrelation < 0 || m_minimumSignedCorrelation > 1.0)
            HART_THROW_OR_RETURN (
                hart::ValueError,
                "Signed correlation threshold should be in 0..1 range",
                false
            );

        if (m_maxLagSeconds < 0)
            HART_THROW_OR_RETURN (
                hart::ValueError,
                "Max lag should be a non-negative number in seconds",
                false
            );
    }

    void prepare (
        double sampleRateHz,
        size_t numInputChannels,
        size_t numOutputChannels,
        size_t /*maxBlockSizeFrames*/
        ) override
    {
        hassert (numInputChannels == numOutputChannels);
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
        m_bestSignedCorrelation = 0.0;
        m_bestLagFrames = 0;
        m_hadValidData = false;
    }

    bool supportsChannelLayout (size_t numInputChannels, size_t numOutputChannels) const override
    {
        return numInputChannels == numOutputChannels;
    }

    bool match (const AudioBuffer<SampleType>& inputAudio, const AudioBuffer<SampleType>& observedOutputAudio) override
    {
        hassert (inputAudio.getNumChannels() == observedOutputAudio.getNumChannels());
        hassert (inputAudio.getNumFrames() == observedOutputAudio.getNumFrames());
        hassert (inputAudio.getSampleRateHz() == observedOutputAudio.getSampleRateHz());

        const size_t numFrames = inputAudio.getNumFrames();

        if (numFrames == 0)
        {
            m_hadValidData = false;
            return false;
        }
        
        const size_t numChannels = inputAudio.getNumChannels();
        bool anyValidChannel = false;

        for (size_t channel = 0; channel < numChannels; ++channel)
        {
            if (! this->appliesToChannel (channel))
                continue;

            const SampleType* x = inputAudio[channel];
            const SampleType* y = observedOutputAudio[channel];
            std::vector<double> prefixSumsSqX (numFrames + 1, 0.0);
            std::vector<double> prefixSumsSqY (numFrames + 1, 0.0);
            AccurateSum<double> runningSumSqX { 0.0 };
            AccurateSum<double> runningSumSqY { 0.0 };

            for (size_t frame = 0; frame < numFrames; ++frame)
            {
                const double xVal = static_cast<double> (x[frame]);
                const double yVal = static_cast<double> (y[frame]);

                runningSumSqX += xVal * xVal;
                runningSumSqY += yVal * yVal;
                prefixSumsSqX[frame + 1] = runningSumSqX;
                prefixSumsSqY[frame + 1] = runningSumSqY;
            }

            double bestAbsCorrelation = -hart::inf;
            double bestSignedCorrelation = 0.0;
            long long int bestLag = 0;
            bool channelValid = false;

            for (long long int lag = -m_maxLagFrames; lag <= m_maxLagFrames; ++lag)
            {
                AccurateSum<double> dotProduct { 0.0 };
                const bool lagShiftsOutputToTheLeft = lag < 0;
                const size_t lagAbsFrames = static_cast<size_t> (lagShiftsOutputToTheLeft ? -lag : lag);

                if (lagAbsFrames >= numFrames)
                    continue;

                // For a given lag, correlate only the valid overlap interval:
                // x[inputOverlapBeginFrame + offset] with y[outputOverlapBeginFrame + offset].
                const size_t inputOverlapBeginFrame = lagShiftsOutputToTheLeft ? lagAbsFrames : 0;
                const size_t outputOverlapBeginFrame = lagShiftsOutputToTheLeft ? 0 : lagAbsFrames;
                const size_t overlapSizeFrames = numFrames - lagAbsFrames;
                const size_t inputOverlapEndFrame = inputOverlapBeginFrame + overlapSizeFrames;
                const size_t outputOverlapEndFrame = outputOverlapBeginFrame + overlapSizeFrames;
                const double sumSqX = prefixSumsSqX[inputOverlapEndFrame] - prefixSumsSqX[inputOverlapBeginFrame];
                const double sumSqY = prefixSumsSqY[outputOverlapEndFrame] - prefixSumsSqY[outputOverlapBeginFrame];

                for (size_t overlapFrame = 0; overlapFrame < overlapSizeFrames; ++overlapFrame)
                {
                    const double inputValue = static_cast<double> (x[inputOverlapBeginFrame + overlapFrame]);
                    const double outputValue = static_cast<double> (y[outputOverlapBeginFrame + overlapFrame]);
                    dotProduct += inputValue * outputValue;
                }

                if (floatsEqual (sumSqX, 0.0) || floatsEqual (sumSqY, 0.0))
                    continue;

                channelValid = true;
                const double corr = dotProduct / std::sqrt (sumSqX * sumSqY);
                const double absCorr = std::abs (corr);

                if (absCorr > bestAbsCorrelation)
                {
                    bestAbsCorrelation = absCorr;
                    bestSignedCorrelation = corr;
                    bestLag = lag;
                }

                if (floatsEqual (absCorr, 1.0))
                    break;
            }

            if (! channelValid)
            {
                if (m_silencePolicy == SilencePolicy::strict)
                {
                    m_hadValidData = false;
                    m_failureChannel = channel;
                    m_failureFrame = 0;
                    return false;
                }

                continue;
            }

            anyValidChannel = true;

            if (bestSignedCorrelation < m_minimumSignedCorrelation)
            {
                m_hadValidData = true;
                m_failureChannel = channel;
                m_failureFrame = 0;
                m_bestSignedCorrelation = bestSignedCorrelation;
                m_bestLagFrames = bestLag;
                return false;
            }
        }

        if (! anyValidChannel)
        {
            m_hadValidData = false;
            m_failureChannel = 0;
            m_failureFrame = 0;
            return false;
        }

        return true;
    }

    MatcherFailureDetails getFailureDetails() const override
    {
        MatcherFailureDetails details;
        details.channel = m_failureChannel;
        details.frame = m_failureFrame;

        if (!m_hadValidData)
        {
            details.description = "Polarity could not be determined with sufficient confidence";
            return details;
        }

        const double lagSeconds = m_bestLagFrames / m_sampleRateHz;
        std::stringstream stream;

        stream
            << "Detected signed correlation: "
            << correlationPrecision << m_bestSignedCorrelation
            << " at lag " << m_bestLagFrames << " frames ("
            << secPrecision << lagSeconds << " seconds)";

        details.description = stream.str();
        return details;
    }

    void represent (std::ostream& stream) const override
    {
        stream
            << "PolarityPreserved ("
            << correlationPrecision << m_minimumSignedCorrelation << ", "
            << secPrecision << m_maxLagSeconds << "_s, "
            << "SilencePolicy::"
            << (m_silencePolicy == SilencePolicy::strict ? "strict" : "relaxed")
            << ")";
    }

private:
    const double m_minimumSignedCorrelation;
    const double m_maxLagSeconds;
    const SilencePolicy m_silencePolicy;

    double m_sampleRateHz = 0.0;
    long long int m_maxLagFrames = 0;

    double m_bestSignedCorrelation = 0.0;
    long long int m_bestLagFrames = 0;

    size_t m_failureChannel = 0;
    size_t m_failureFrame = 0;
    bool m_hadValidData = false;
};

HART_MATCHER_DECLARE_ALIASES_FOR (PolarityPreserved)

} // namespace hart
