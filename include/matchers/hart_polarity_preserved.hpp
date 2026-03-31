#pragma once

#include <cmath>
#include <iomanip>
#include <sstream>

#include "hart_exceptions.hpp"
#include "hart_matcher.hpp"
#include "hart_precision.hpp"
#include "hart_utils.hpp"

namespace hart
{

template <typename SampleType>
class PolarityPreserved :
    public Matcher<SampleType, PolarityPreserved<SampleType>>
{
public:
    enum class SilencePolicy
    {
        Strict,
        Relaxed
    };

    PolarityPreserved (double minimumSignedCorrelation = 0.5, double maxLagSeconds = 0.01, SilencePolicy silencePolicy = SilencePolicy::Strict):
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
        size_t /*numChannels*/,
        size_t /*maxBlockSizeFrames*/
        ) override
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
        m_bestSignedCorrelation = 0.0;
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

        bool anyValidChannel = false;

        for (size_t channel = 0; channel < numChannels; ++channel)
        {
            if (! this->appliesToChannel (channel))
                continue;

            const SampleType* x = inputAudio[channel];
            const SampleType* y = observedOutputAudio[channel];

            double bestAbsCorrelation = -hart::inf;
            double bestSignedCorrelation = 0.0;
            long long int bestLag = 0;
            bool channelValid = false;

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
                if (m_silencePolicy == SilencePolicy::Strict)
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
            << (m_silencePolicy == SilencePolicy::Strict ? "Strict" : "Relaxed")
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
