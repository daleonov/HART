#pragma once

#include <functional>
#include <memory>

#include "hart_audio_buffer.hpp"
#include "hart_matcher_failure_details.hpp"

namespace hart
{

/// @brief Private interface for latency detector implementations for the hart::LatencyBelow Matcher
/// @private
template <typename SampleType>
class LatencyDetector
{
public:
    virtual ~LatencyDetector() = default;
    virtual void prepare (double sampleRateHz, size_t numChannels, size_t maxBlockSizeFrames) = 0;
    virtual void reset() = 0;
    virtual bool match (
        const AudioBuffer<SampleType>& inputAudio,
        const AudioBuffer<SampleType>& observedOutputAudio,
        const std::function<bool (size_t)>& appliesToChannel
        ) = 0;
    virtual MatcherFailureDetails getFailureDetails() const = 0;
    virtual std::unique_ptr<LatencyDetector<SampleType>> copy() const = 0;
};

} // namespace hart
