#pragma once

#include <sstream>
#include <vector>

#include "hart_accurate_sum.hpp"
#include "hart_latency_detector.hpp"
#include "hart_precision.hpp"
#include "hart_silence_policy.hpp"
#include "hart_utils.hpp"  // make_unique(), roundToSizeT(), inf

namespace hart
{

/// @brief Correlation-based latency detector implementation for the hart::LatencyBelow class. For internal use.
/// @private
template <typename SampleType>
class CorrelationLatencyDetector :
    public LatencyDetector<SampleType>
{
public:
    CorrelationLatencyDetector (double maxLatencySeconds, SilencePolicy silencePolicy, double absCorrelationThreshold) :
        m_maxLatencySeconds (maxLatencySeconds),
        m_silencePolicy (silencePolicy),
        m_absCorrelationThreshold (absCorrelationThreshold)
    {
        // Values that are <= 0 are supposed to be treated as a "default value" sentinel, and get replaced with actual default value
        hassert (absCorrelationThreshold > 0.0);
    }

    std::unique_ptr<LatencyDetector<SampleType>> copy() const override
    {
        return hart::make_unique<CorrelationLatencyDetector<SampleType>> (*this);
    }

    void prepare (double sampleRateHz, size_t /* numChannels */, size_t /* maxBlockSizeFrames */) override
    {
        m_sampleRateHz = sampleRateHz;
    }

    void reset() override
    {
        m_hadValidData = false;
        m_failureChannel = 0;
        m_failureFrame = 0;
        m_detectedLatencyFrames = 0;
        m_bestCorrelation = 0.0;
    }

    bool match (
        const AudioBuffer<SampleType>& inputAudio,
        const AudioBuffer<SampleType>& observedOutputAudio,
        const std::function<bool (size_t)>& appliesToChannel
        ) override
    {
        const size_t numFrames = inputAudio.getNumFrames();
        const size_t maxLagFrames = numFrames - 1;
        bool anyValidChannel = false;
        size_t worstLatencyFrames = 0;
        size_t worstChannel = 0;

        for (size_t channel = 0; channel < inputAudio.getNumChannels(); ++channel)
        {
            if (! appliesToChannel (channel))
                continue;

            const SampleType* x = inputAudio[channel];
            const SampleType* y = observedOutputAudio[channel];

            double bestAbsCorrelation = -hart::inf;
            size_t bestLag = 0;
            bool channelValid = false;

            for (size_t lag = 0; lag <= maxLagFrames; ++lag)
            {
                AccurateSum<SampleType> dotProduct = 0.0;
                AccurateSum<SampleType> sumSqX = 0.0;
                AccurateSum<SampleType> sumSqY = 0.0;
                size_t overlapCount = 0;

                for (size_t n = 0; n + lag < numFrames; ++n)
                {
                    const size_t yn = n + lag;
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
                const double correlation = static_cast<double> (dotProduct) / std::sqrt (static_cast<double> (sumSqX * sumSqY));
                const double absCorrelation = std::abs (correlation);

                if (absCorrelation > bestAbsCorrelation)
                {
                    bestAbsCorrelation = absCorrelation;
                    bestLag = lag;
                }

                if (floatsEqual (bestAbsCorrelation, 1.0))
                    break;
            }

            if (! channelValid || bestAbsCorrelation < m_absCorrelationThreshold)
            {
                if (m_silencePolicy == SilencePolicy::Strict)
                {
                    m_hadValidData = false;
                    m_failureChannel = channel;
                    return false;
                }

                continue;
            }

            anyValidChannel = true;

            if (bestLag > worstLatencyFrames)
            {
                worstLatencyFrames = bestLag;
                worstChannel = channel;
                m_bestCorrelation = bestAbsCorrelation;
            }
        }

        if (!anyValidChannel)
        {
            m_hadValidData = false;
            return false;
        }

        m_hadValidData = true;
        m_detectedLatencyFrames = worstLatencyFrames;
        const double latencySeconds = m_detectedLatencyFrames / m_sampleRateHz;

        if (latencySeconds <= m_maxLatencySeconds)
            return true;

        m_failureChannel = worstChannel;
        return false;
    }

    MatcherFailureDetails getFailureDetails() const override
    {
        MatcherFailureDetails details;
        details.channel = m_failureChannel;
        details.frame = m_failureFrame;

        if (! m_hadValidData)
        {
            details.description = "Latency could not be determined with sufficient correlation";
            return details;
        }

        const double latencySeconds = m_detectedLatencyFrames / m_sampleRateHz;

        std::stringstream descriptionStream;
        descriptionStream
            << "Detected latency: "
            << secPrecision << latencySeconds << " seconds ("
            << m_detectedLatencyFrames << " frames), "
            << "best correlation: " << correlationPrecision << m_bestCorrelation;

        details.description = descriptionStream.str();
        return details;
    }

private:
    const double m_maxLatencySeconds;
    const SilencePolicy m_silencePolicy;
    const double m_absCorrelationThreshold;
    double m_sampleRateHz = 0.0;

    bool m_hadValidData = false;
    size_t m_detectedLatencyFrames = 0;
    double m_bestCorrelation = 0.0;
    size_t m_failureChannel = 0;
    size_t m_failureFrame = 0;
};

} // namespace hart