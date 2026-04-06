#pragma once

#include <sstream>
#include <vector>

#include "hart_accurate_sum.hpp"
#include "hart_exceptions.hpp"
#include "hart_latency_detector.hpp"
#include "hart_precision.hpp"
#include "hart_silence_policy.hpp"
#include "hart_utils.hpp"  // make_unique(), roundToSizeT(), inf, floatsEqual()

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
        if (absCorrelationThreshold > 1.0)
            HART_THROW_OR_RETURN_VOID (hart::ValueError, "Normalized correlation threshold can not be higher than 1");

        if (absCorrelationThreshold < 0.0)
            HART_THROW_OR_RETURN_VOID (hart::ValueError, "Correlation threshold is an absolute value, so should not be negative");

        // Technically, zero correlation is okay, but a bit too weird...
        if (floatsEqual (absCorrelationThreshold, 0.0))
            HART_THROW_OR_RETURN_VOID (hart::ValueError, "Zero correlation threshold is not a meaningful value in latency detector context");
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

        if (numFrames == 0)
        {
            m_hadValidData = false;
            return false;
        }

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
            size_t bestLag = 0;
            bool channelValid = false;

            for (size_t lag = 0; lag <= maxLagFrames; ++lag)
            {
                AccurateSum<double> dotProduct = { 0.0 };
                const size_t inputOverlapBeginFrame = 0;
                const size_t outputOverlapBeginFrame = lag;
                const size_t overlapSizeFrames = numFrames - lag;
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
                const double correlation = dotProduct / std::sqrt (sumSqX * sumSqY);
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
                if (m_silencePolicy == SilencePolicy::strict)
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
