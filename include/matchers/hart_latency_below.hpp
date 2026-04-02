#pragma once

#include <cmath>  // isnan()
#include <memory>

#include "matchers/hart_correlation_latency_detector.hpp"
#include "matchers/hart_latency_detector.hpp"
#include "matchers/hart_matcher.hpp"
#include "matchers/hart_onset_latency_detector.hpp"
#include "hart_precision.hpp"
#include "hart_silence_policy.hpp"
#include "hart_utils.hpp"  // make_unique(), nan()

namespace hart
{
/// @brief Checks whether the output signal latency is below a specified amount
/// @details
/// Compares input and output audio and measures the observed latency between them.
/// In a multi-channel setup, latency is measured independently for every applicable
/// channel, and the largest detected latency is used.
///
/// The latency detection method is selected via `Method`:
/// - `Method::onset` detects latency as a difference between the first threshold crossing
///   in the input and output signals.
/// - `Method::correlation` uses normalized cross-correlation and finds the lag that best
///   aligns the output waveform to the input.
///
/// `SilencePolicy` defines how the matcher behaves when latency cannot be reliably
/// determined on one or more channels.
///
/// @tparam SampleType Type of audio samples, typically `float` or `double`
/// @ingroup Matchers
template <typename SampleType>
class LatencyBelow :
    public Matcher<SampleType, LatencyBelow<SampleType>>
{
public:
    /// @brief Selects the method used to detect latency
    enum class Method
    {
        onset,  ///< Detect latency from the first threshold crossing in input and output
        correlation  ///< Detect latency using best waveform alignment via cross-correlation
    };

    /// @brief Creates a matcher that expects latency between input and output below a specified value
    /// @param maxLatencySeconds Maximum allowed detected latency in seconds
    /// @param method Latency detection method. See `Method` for details.
    /// @param threshold Method-specific detection threshold:
    /// - for `Method::onset`, this is the minimal sample level in dB required to detect onset.
    /// - for `Method::correlation`, this is the minimum absolute correlation required for latency
    ///   detection to be considered valid
    ///
    /// NaN value will trigger a sensible method-specific default, namely:
    /// - `-120` (in dB) as sample value threshold for onset-based detection
    /// - `0.5` as absolute normalized cross-correlation value threshold for correlation-based detection
    /// @param silencePolicy Defines how channels with insufficient measurable signal are handled:
    /// - `SilencePolicy::strict` fails if any applicable channel cannot produce a valid latency estimate
    /// - `SilencePolicy::relaxed` ignores such channels and uses valid ones only
    LatencyBelow (
        double maxLatencySeconds,
        Method method = Method::onset,
        double threshold = hart::nan<double>(),
        SilencePolicy silencePolicy = SilencePolicy::strict
        ) :
        m_maxLatencySeconds (maxLatencySeconds),
        m_method (method),
        m_silencePolicy (silencePolicy)
    {
        const bool thresholdDefaultValueRequested = std::isnan (threshold);

        if (method == Method::onset)
        {
            m_threshold = thresholdDefaultValueRequested ? -120.0 : threshold;
            m_latencyDetector = hart::make_unique<OnsetLatencyDetector<SampleType>> (
                maxLatencySeconds,
                silencePolicy,
                m_threshold
                );
        }
        else
        {
            m_threshold = thresholdDefaultValueRequested ? 0.5 : threshold;
            m_latencyDetector = hart::make_unique<CorrelationLatencyDetector<SampleType>> (
                maxLatencySeconds,
                silencePolicy,
                m_threshold
                );
        }

        // Sanity checks
        hassert (m_latencyDetector != nullptr);
        hassert (! std::isnan (m_threshold));
    }

    LatencyBelow (const LatencyBelow& other):
        m_maxLatencySeconds (other.m_maxLatencySeconds),
        m_silencePolicy (other.m_silencePolicy),
        m_threshold (other.m_threshold),
        m_latencyDetector (
            other.m_latencyDetector != nullptr
                ? other.m_latencyDetector->copy()
                : nullptr
        )
    {
        hassert (other.m_latencyDetector != nullptr);  // Sanity check
    }

    LatencyBelow (LatencyBelow&& other) noexcept = default;

    LatencyBelow& operator= (const LatencyBelow& other)
    {
        if (this == &other)
            return *this;

        hassert (other.m_latencyDetector != nullptr);  // Sanity check

        m_maxLatencySeconds = other.m_maxLatencySeconds;
        m_silencePolicy = other.m_silencePolicy;
        m_threshold = other.m_threshold;

        m_latencyDetector =
            other.m_latencyDetector != nullptr
                ? other.m_latencyDetector->copy()
                : nullptr;

        return *this;
    }

    LatencyBelow& operator= (LatencyBelow&& other) noexcept = default;

    ~LatencyBelow() override = default;

    void prepare (double sampleRateHz, size_t numChannels, size_t maxBlockSizeFrames) override
    {
        hassert (m_latencyDetector != nullptr);
        m_latencyDetector->prepare (
            sampleRateHz,
            numChannels,
            maxBlockSizeFrames
        );
    }

    bool canOperatePerBlock() const override
    {
        return false;
    }

    void reset() override
    {
        hassert (m_latencyDetector != nullptr);
        m_latencyDetector->reset();
    }

    bool match (const AudioBuffer<SampleType>& inputAudio, const AudioBuffer<SampleType>& observedOutputAudio) override
    {
        hassert (m_latencyDetector != nullptr);
        return m_latencyDetector->match (
            inputAudio,
            observedOutputAudio,
            [this] (size_t channel) { return this->appliesToChannel (channel); }
        );
    }

    MatcherFailureDetails getFailureDetails() const override
    {
        hassert (m_latencyDetector != nullptr);
        return m_latencyDetector->getFailureDetails();
    }

    void represent (std::ostream& stream) const override
    {
        stream
            << "LatencyBelow ("
            << secPrecision << m_maxLatencySeconds << "_s, ";

        if (m_method == Method::onset)
        {
            stream
                << "Method::onset, "
                << dbPrecision << m_threshold << "_dB, ";
        }
        else {
            stream
                << "Method::correlation, "
                << correlationPrecision << m_threshold << ", ";
        }

        stream
            << "SilencePolicy::" << (m_silencePolicy == SilencePolicy::strict ? "strict" : "relaxed")
            << ')';
    }

private:
    double m_maxLatencySeconds;
    Method m_method;
    SilencePolicy m_silencePolicy;
    SampleType m_threshold;

    std::unique_ptr<LatencyDetector<SampleType>> m_latencyDetector;
};

HART_MATCHER_DECLARE_ALIASES_FOR (LatencyBelow)

} // namespace hart
