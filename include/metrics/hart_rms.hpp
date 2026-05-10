#pragma once

#include <cmath>  // sqrt()

#include "hart_accurate_sum.hpp"
#include "hart_audio_buffer.hpp"
#include "hart_exceptions.hpp"
#include "metrics/hart_metric_query.hpp"
#include "metrics/hart_metrics_common.hpp"  // ChannelSubsets
#include "hart_slice.hpp"
#include "hart_units.hpp"  // Unit
#include "hart_utils.hpp"  // nan(), ratioToDecibels()

namespace hart
{

/// @brief  Calculates root mean square (RMS) of a signal
/// @details  RMS a metric that expresses the average magnitude, or effective
/// energy level, of an audio signal over time. It is commonly used to estimate
/// perceived loudness, and to measure overall signal level.
/// 
/// RMS is calculated this way:
/// @f[
/// \mathrm{RMS} = \sqrt{\frac{1}{N} \sum_{n=0}^{N-1} x[n]^2}
/// @f]
/// (RMS = sqrt((1 / N) * sum(x[n] ** 2))),
///
/// where x[n] is audio sample value from one channel, and N
/// is number of frames in the provided buffer.
///
/// Can be expressed as ratio or decibels, supports `Unit:::native`,
/// `Unit::linear`, `Unit::dB` units. Value in decibels is
/// calculated as a ratio, not power.
///
/// @tparam SampleType 
/// @param buffer Audio buffer to calculate RMS at
/// @return Chainable `MetricQuery` object, which calculates RMS as linear ratio
/// or decibels
template <typename SampleType>
MetricQuery<double> rms (const AudioBuffer<SampleType>& buffer)
{
    typename MetricQuery<double>::SingleChannelMetricEvaluator evaluator =
        [&buffer]
        (size_t channel, Slice slice, Unit requestedUnit)
        -> double
    {
        hassert (channel < buffer.getNumChannels());

        if (slice.isEmpty())
            return hart::nan<double>();

        const auto sliceFrameIndices = buffer.getFrameIndices (slice);
        const size_t sliceStart = sliceFrameIndices.first;
        const size_t sliceStop = sliceFrameIndices.second;
        const size_t numFrames = sliceStop - sliceStart;
        hassert (numFrames != 0);
        hassert (sliceStart < sliceStop);
        hassert (sliceStop <= buffer.getNumFrames());

        const SampleType* channelData = buffer[channel];
        AccurateSum<SampleType> sumSquares;

        for (size_t frame = sliceStart; frame < sliceStop; ++frame)
        {
            const double x = channelData[frame];
            sumSquares += x * x;
        }

        const double rmsLinear = std::sqrt (sumSquares.get<double>() / numFrames);

        switch (requestedUnit)
        {
            case Unit::native:
            case Unit::linear: return rmsLinear;

            case Unit::dB: return hart::ratioToDecibels (rmsLinear);

            default: HART_THROW_OR_RETURN (hart::UnitError, "Unsupported unit",  hart::nan<double>());
        }
    };

    const size_t numChannels = buffer.getNumChannels();
    return MetricQuery<double> (
        std::move (evaluator),
        numChannels,
        ChannelSubsets::allChannels (numChannels)
    );
}

}  // namespace hart
