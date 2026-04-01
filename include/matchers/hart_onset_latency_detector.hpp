#pragma once

#include <vector>
#include <utility>  // pair

#include "hart_latency_detector.hpp"
#include "hart_precision.hpp"
#include "hart_silence_policy.hpp"
#include "hart_utils.hpp"  // floatsEqual(), make_unique()

namespace hart
{

/// @brief Onset-based latency detector implementation for the hart::LatencyBelow class. For internal use.
/// @private
template <typename SampleType>
class OnsetLatencyDetector :
    public LatencyDetector<SampleType>
{
public:
    OnsetLatencyDetector (double maxLatencySeconds, SilencePolicy silencePolicy, double absThresholdLinear):
        m_maxLatencySeconds (maxLatencySeconds),
        m_silencePolicy (silencePolicy),
        m_absThresholdLinear (absThresholdLinear)
    {
        // Values that are <= 0 are supposed to be treated as a "default value" sentinel, and get replaced with actual default value
        hassert (absThresholdLinear > (SampleType) 0.0);
    }

    void prepare (
        double sampleRateHz,
        size_t numChannels,
        size_t /*maxBlockSizeFrames*/
        ) override
    {
        m_sampleRateHz = sampleRateHz;
        m_numChannels = numChannels;
    }

    void reset() override
    {
        m_noOnsetsDetected = false;
        m_allChannelsFailed = false;
        m_detectedLatencyFrames = 0;
        m_detectedInputOnsetFrames = 0;
        m_detectedOutputOnsetFrames = 0;
        m_failureChannel = 0;
        m_failureFrame = 0;
    }

    bool match (
        const AudioBuffer<SampleType>& inputAudio,
        const AudioBuffer<SampleType>& observedOutputAudio,
        const std::function<bool (size_t)>& appliesToChannel
        ) override
    {
        const size_t numChannels = inputAudio.getNumChannels();
        std::vector<long long int> validLatenciesFrames;
        std::vector<size_t> usedChannels;
        std::vector<size_t> ignoredChannels;
        std::vector<std::pair<size_t, size_t>> onsetsFrames (numChannels);

        for (size_t channel = 0; channel < numChannels; ++channel)
        {
            if (! appliesToChannel (channel))
                continue;

            const OnsetDetails inputOnset = findOnsetFrame (inputAudio, channel);
            const OnsetDetails outputOnset = findOnsetFrame (observedOutputAudio, channel);

            if (inputOnset.found && outputOnset.found)
            {
                // It can be negative, in theory
                const long long int latencyFrames =
                    static_cast<long long int> (outputOnset.frame) - inputOnset.frame;

                validLatenciesFrames.push_back (latencyFrames);
                usedChannels.push_back (channel);
                onsetsFrames[channel] = { inputOnset.frame, outputOnset.frame };
            }
            else
            {
                ignoredChannels.push_back (channel);

                if (m_silencePolicy == SilencePolicy::strict)
                {
                    m_noOnsetsDetected = true;
                    m_allChannelsFailed = false;
                    m_failureChannel = channel;
                    m_detectedInputOnsetFrames = 0;
                    m_detectedOutputOnsetFrames = 0;
                    m_failureFrame = 0;

                    return false;
                }
            }
        }

        if (validLatenciesFrames.empty())
        {
            m_noOnsetsDetected = true;
            m_allChannelsFailed = true;
            m_failureChannel = 0;
            m_detectedInputOnsetFrames = 0;
            m_detectedOutputOnsetFrames = 0;
            m_failureFrame = 0;

            return false;
        }

        // Pick the worst-case latency
        long long int latencyFrames = validLatenciesFrames[0];
        size_t latencyChannel = usedChannels[0];

        for (size_t i = 1; i < validLatenciesFrames.size(); ++i)
        {
            if (validLatenciesFrames[i] > latencyFrames)
            {
                latencyFrames = validLatenciesFrames[i];
                latencyChannel = usedChannels[i];
            }
        }

        m_detectedLatencyFrames = latencyFrames;
        const double detectedLatencySeconds = latencyFrames / m_sampleRateHz;

        if (detectedLatencySeconds <= m_maxLatencySeconds)
            return true;

        m_noOnsetsDetected = false;
        m_failureChannel = latencyChannel;
        m_detectedInputOnsetFrames = onsetsFrames[latencyChannel].first;
        m_detectedOutputOnsetFrames = onsetsFrames[latencyChannel].second;
        m_failureFrame = m_detectedOutputOnsetFrames;

        return false;
    }

    MatcherFailureDetails getFailureDetails() const override
    {
        MatcherFailureDetails details;
        details.frame = m_failureFrame;  // This is an output audio's frame, at which the signal onset was detected
        details.channel = m_failureChannel;
        std::stringstream detailsStream;

        if (m_noOnsetsDetected)
        {
            detailsStream
                << "Latency could not be determined: "
                << (m_allChannelsFailed ? "no channels exceeded threshold" : "one of the channels does not exceed threshold");
        }
        else
        {
            const double detectedLatencySeconds = m_detectedLatencyFrames / m_sampleRateHz;

            detailsStream
                << "Detected latency: "
                << secPrecision << detectedLatencySeconds << " seconds ("
                << m_detectedLatencyFrames << " frames)";

            if (m_detectedInputOnsetFrames != 0)
            {
                const double detectedInputOnsetSeconds = m_detectedInputOnsetFrames / m_sampleRateHz;
                const double detectedOutputOnsetSeconds = m_detectedOutputOnsetFrames / m_sampleRateHz;

                detailsStream
                    << ",\nInput onset: "
                    << detectedInputOnsetSeconds << " seconds ("
                    << m_detectedInputOnsetFrames << " frames),\n"
                    << "Output onset: "
                    << detectedOutputOnsetSeconds << " seconds ("
                    << m_detectedOutputOnsetFrames << " frames)\n";
            }

        }

        details.description = detailsStream.str();
        return details;
    }

    std::unique_ptr<LatencyDetector<SampleType>> copy() const override
    {
        return hart::make_unique<OnsetLatencyDetector<SampleType>> (*this);
    }

private:
    struct OnsetDetails
    {
        bool found;
        size_t frame;
    };

    const double m_maxLatencySeconds;
    const SilencePolicy m_silencePolicy;
    const double m_absThresholdLinear;

    double m_sampleRateHz = 0.0;
    size_t m_numChannels = 0;

    bool m_noOnsetsDetected = false;
    bool m_allChannelsFailed = false;
    long long int m_detectedLatencyFrames = 0;
    size_t m_detectedInputOnsetFrames = 0;
    size_t m_detectedOutputOnsetFrames = 0;
    size_t m_failureChannel = 0;
    size_t m_failureFrame = 0;

    OnsetDetails findOnsetFrame (const AudioBuffer<SampleType>& buffer, size_t channel) const
    {
        const size_t numFrames = buffer.getNumFrames();

        for (size_t frame = 0; frame < numFrames; ++frame)
            if (std::abs (static_cast<double> (buffer[channel][frame])) > m_absThresholdLinear)
                return { true, frame };

        // TODO: Put "non applicable" frame value here
        return { false, 0 };
    }
};

} // namespace hart
