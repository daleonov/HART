#pragma once

#include "hart_audio_buffer.hpp"
#include "hart_exceptions.hpp"
#include "metrics/hart_metric_query.hpp"
#include "metrics/hart_metrics_common.hpp"  // ChannelSubsets
#include "hart_slice.hpp"
#include "hart_units.hpp"  // Unit
#include "hart_utils.hpp"  // floatsEqual(), nan()

namespace hart
{

/// @brief  Calculates zero-crossing rate (ZCR) of a signal
/// @details Useful to estimate frequency of stationary monophonic signals.
/// Supports `Unit::native` and `Unit::Hz` units, which both result in the same value.
/// @tparam SampleType type of audio buffer data, typically float or double
/// @param buffer Audio buffer to calculate ZCR at
/// @return Chainable `MetricQuery` object, which calculates RMS as linear ratio
/// or decibels
/// @ingroup Metrics
template <typename SampleType>
MetricQuery<double> zcr (const AudioBuffer<SampleType>& buffer)
{
    if (! buffer.hasSampleRate())
        HART_THROW_OR_RETURN (hart::SampleRateError, "Audio buffer must have sample rate metadata", {});

    if (floatsEqual (buffer.getSampleRateHz(), 0.0))
        HART_THROW_OR_RETURN (hart::SampleRateError, "Audio buffer's sample rate should not be zero", {});

    typename MetricQuery<double>::SingleChannelMetricEvaluator evaluator =
        [&buffer]
        (size_t channel, Slice slice, Unit requestedUnit)
        -> double
    {
        hassert (channel < buffer.getNumChannels());

        if (requestedUnit != Unit::native && requestedUnit != Unit::Hz)
            HART_THROW_OR_RETURN (hart::UnitError, "Unsupported unit",  hart::nan<double>());

        if (slice.isEmpty())
            return hart::nan<double>();

        const auto sliceFrameIndices = buffer.getFrameIndices (slice);
        const size_t sliceStart = sliceFrameIndices.first;
        const size_t sliceStop = sliceFrameIndices.second;
        const size_t numFrames = sliceStop - sliceStart;
        hassert (numFrames != 0);
        hassert (sliceStart < sliceStop);
        hassert (sliceStop <= buffer.getNumFrames());

        if (numFrames <= 2)
            return hart::nan<double>();

        const SampleType* channelData = buffer[channel];
        size_t zeroCrossings = 0;
        double previous = channelData[sliceStart];

        for (size_t frame = sliceStart + 1; frame < sliceStop; ++frame)
        {
            const double current = channelData[frame];
            zeroCrossings += (previous >= 0.0) ^ (current >= 0.0);
            previous = current;
        }

        const double sliceDurationSeconds = static_cast<double> (numFrames) / buffer.getSampleRateHz();
        return static_cast<double> (zeroCrossings) / sliceDurationSeconds;
    };

    const size_t numChannels = buffer.getNumChannels();
    return MetricQuery<double> (
        std::move (evaluator),
        numChannels,
        ChannelSubsets::allChannels (numChannels)
    );
}

}  // namespace hart
