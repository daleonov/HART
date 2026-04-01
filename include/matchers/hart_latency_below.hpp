#pragma once

#include <memory>

#include "matchers/hart_correlation_latency_detector.hpp"
#include "matchers/hart_latency_detector.hpp"
#include "matchers/hart_matcher.hpp"
#include "matchers/hart_onset_latency_detector.hpp"
#include "hart_precision.hpp"
#include "hart_silence_policy.hpp"
#include "hart_utils.hpp"  // make_unique()

// TODO: Add docs for both methods

namespace hart
{

template <typename SampleType>
class LatencyBelow :
    public Matcher<SampleType, LatencyBelow<SampleType>>
{
public:
    enum class Method
    {
        onset,
        correlation
    };

    LatencyBelow (
        double maxLatencySeconds,
        Method method = Method::onset,
        double threshold = 0.0,
        SilencePolicy silencePolicy = SilencePolicy::strict
        ) :
        m_maxLatencySeconds (maxLatencySeconds),
        m_silencePolicy (silencePolicy),
        m_method (method)
    {
        const bool thresholdDefaultValueRequested = threshold <= 0.0;

        if (method == Method::onset)
        {
            m_threshold = thresholdDefaultValueRequested ? 1e-6 : threshold;
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
                static_cast<SampleType> (m_threshold)
                );
        }

        // Sanity checks
        hassert (m_latencyDetector != nullptr);
        hassert (m_threshold > 0.0);
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
            << secPrecision << m_maxLatencySeconds << "_s, "
            << "SilencePolicy::" << (m_silencePolicy == SilencePolicy::strict ? "strict, " : "relaxed, ")
            << (m_method == Method::onset ? linPrecision : correlationPrecision)
            << m_threshold << ", "
            << "Method::" << (m_method == Method::onset ? "onset" : "correlation")
            << ')';
    }

private:
    double m_maxLatencySeconds;
    SilencePolicy m_silencePolicy;
    SampleType m_threshold;
    Method m_method;

    std::unique_ptr<LatencyDetector<SampleType>> m_latencyDetector;
};

HART_MATCHER_DECLARE_ALIASES_FOR (LatencyBelow)

} // namespace hart
