#pragma once

#include <vector>
#include <utility>  // pair

#include "hart_matcher.hpp"
#include "hart_precision.hpp"  // secPrecision, linPrecision
#include "hart_str.hpp"

namespace hart
{

/// @brief Checks whether the output signal has a latency under a specified amount, compared to the input
/// @details The latency is determined as a difference between an input signal's onset and the output's
/// onset. And onset is the point where absolute value of the sample crosses the threshold upwards for
/// the first time. In a multi-channel setup, it keeps tracks of latencies on each individual channel, and
/// takes the largest one into account.
/// tparam SampleType Type of audio samples, typically `float` or `double`
/// @ingroup Matchers
template <typename SampleType>
class LatencyBelow :
    public Matcher<SampleType, LatencyBelow<SampleType>>
{
public:
    /// @brief Defines behaviour when onset cannot be detected on one of the channels
    enum class SilencePolicy
    {
        Strict,  ///< Matcher reports failure if even one of the channels does not have an onset
        Relaxed  ///< Matcher ignores the channels that do not have an onset
    };

    /// @brief Creates a matcher that expects a latency between input and output under a specified value
    /// @param maxLatencySeconds The latency value above which the matcher shoul report a failure
    /// @param silencePolicy What to do if some of the input or output channels are silent, and therefore
    /// the latency there cannot be measured. See `SilencePolicy` for details.
    /// @param thresholdLinear This value determines a threshold the signal has to pass to be detected as onset.
    /// The difference between input and output onset timings is the observed latency in this Matcher.
    LatencyBelow (double maxLatencySeconds, SilencePolicy silencePolicy = SilencePolicy::Strict, SampleType thresholdLinear = (SampleType) 1e-6):
        m_maxLatencySeconds (maxLatencySeconds),
        m_silencePolicy (silencePolicy),
        m_thresholdLinear (thresholdLinear)
    {
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

    bool canOperatePerBlock() const override
    {
        return false;
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

    bool match (const AudioBuffer<SampleType>& inputAudio, const AudioBuffer<SampleType>& observedOutputAudio) override
    {
        const size_t numChannels = inputAudio.getNumChannels();
        std::vector<long long int> validLatenciesFrames;
        std::vector<size_t> usedChannels;
        std::vector<size_t> ignoredChannels;
        std::vector<std::pair<size_t, size_t>> onsetsFrames (numChannels);

        for (size_t channel = 0; channel < numChannels; ++channel)
        {
            if (! this->appliesToChannel (channel))
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

                if (m_silencePolicy == SilencePolicy::Strict)
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

        if (m_noOnsetsDetected)
        {
            details.description = HART_STR (
                "Latency could not be determined: "
                << (m_allChannelsFailed ? "no channels exceeded threshold" : "one of the channels does not exceed threshold")
                );
        }
        else
        {
            const double detectedLatencySeconds = m_detectedLatencyFrames / m_sampleRateHz;
            std::stringstream stream;

            stream
                << "Detected latency: " << secPrecision 
                << detectedLatencySeconds << " seconds ("
                << m_detectedLatencyFrames << " frames)";

            if (m_detectedInputOnsetFrames != 0)
            {
                const double detectedInputOnsetSeconds = m_detectedInputOnsetFrames / m_sampleRateHz;
                const double detectedOutputOnsetSeconds = m_detectedOutputOnsetFrames / m_sampleRateHz;

                stream
                    << ",\nInput onset: "
                    << detectedInputOnsetSeconds << " seconds ("
                    << m_detectedInputOnsetFrames << " frames),\n"
                    << "Output onset: "
                    << detectedOutputOnsetSeconds << " seconds ("
                    << m_detectedOutputOnsetFrames << " frames)\n";
            }

            details.description = stream.str();
        }

        return details;
    }

    void represent (std::ostream& stream) const override
    {
        stream
            << "LatencyBelow ("
            << secPrecision << m_maxLatencySeconds << "_s, "
            << "SilencePolicy::" << (m_silencePolicy == SilencePolicy::Strict ? "Strict, " : "Relaxed, ")
            << linPrecision << m_thresholdLinear
            << ')';
    }

private:
    struct OnsetDetails
    {
        bool found;
        size_t frame;
    };

    const double m_maxLatencySeconds;
    const SilencePolicy m_silencePolicy;
    const SampleType m_thresholdLinear;

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
            if (std::abs (buffer[channel][frame]) > m_thresholdLinear)
                return { true, frame };

        // TODO: Put "non applicable" frame value here
        return { false, 0 };
    }

};

HART_MATCHER_DECLARE_ALIASES_FOR (LatencyBelow)

} // namespace hart
