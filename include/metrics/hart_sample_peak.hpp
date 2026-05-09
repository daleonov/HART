#pragma once

#include <algorithm>  // max()
#include <cmath>  // abs()
#include <vector>

#include "hart_audio_buffer.hpp"
#include "hart_exceptions.hpp"
#include "metrics/hart_metric_query.hpp"
#include "hart_units.hpp"  // Unit
#include "hart_utils.hpp"  // nan

namespace hart
{

/// @brief Calculates Sample Peak of an audio buffer
/// @details Calculates rectified peak values for each channel. Use a reducer to
/// get a scalar value (see @ref Reducers). Supports `Unit::linear` (default) and
/// `Unit::dB` units. Usage example:
/// @code
/// HART_EXPECT_FLOAT_EQ (samplePeak (monoBuffer).as (dB).get(), -3_dB, 1e-2) << "Peaks below 3 dB";
/// HART_EXPECT_LT (samplePeak (monoBuffer).as (linear).get(), 1.0, 1e-3) << "Peaks below unity gain (in linear domain)";
/// HART_EXPECT_LT (samplePeak (stereoBuffer).as (dB).get (max()), -3_dB) << "Loudest channel peaks below 3 dB";
/// @endcode
/// @note It doesn't estimate inter-sample peaks. For true (inter-sample) peaks,
/// consider using the `hart::TruePeaksBelow` matcher.
/// @param audioBuffer Buffer to measure sample peaks in.
/// @throws hart::IndexError if slice's boundary is out of audio buffer's range
/// @throws hart::UnitError if unsupported unit is requested
/// @ingroup Metrics
template <typename SampleType>
MetricQuery<double> samplePeak (const AudioBuffer<SampleType>& audioBuffer)
{
    typename MetricQuery<double>::SingleChannelMetricEvaluator evaluator =
        [&audioBuffer]
        (size_t channel, Slice slice, Unit requestedUnit)
        -> double
    {
        if (slice.isEmpty())
            return hart::nan<double>();

        const auto sliceFrameIndices = audioBuffer.getFrameIndices (slice);
        const size_t sliceStart = sliceFrameIndices.first;
        const size_t sliceStop = sliceFrameIndices.second;
        hassert (sliceStop > sliceStart);
        hassert (sliceStop - sliceStart != 0);
        hassert (sliceStop <= audioBuffer.getNumFrames());

        const SampleType* samples = audioBuffer[channel];
        SampleType peakLinear = 0.0;

        for (size_t frame = sliceStart; frame < sliceStop; ++frame)
            peakLinear = std::max (std::abs (samples[frame]), peakLinear);

        switch (requestedUnit)
        {
            case Unit::native:
            case Unit::linear: return static_cast<double> (peakLinear);

            case Unit::dB: return hart::ratioToDecibels (static_cast<double> (peakLinear));

            default: HART_THROW_OR_RETURN (hart::UnitError, "Unsupported unit",  hart::nan<double>());
        }
    };

    std::vector<size_t> defaultChannelsToProcess (audioBuffer.getNumChannels());

    for (size_t i = 0; i < defaultChannelsToProcess.size(); ++i)
        defaultChannelsToProcess[i] = i;

    return MetricQuery<double> (
        std::move (evaluator),
        audioBuffer.getNumChannels(),
        audioBuffer.getNumFrames(),
        std::move (defaultChannelsToProcess)
    );
}

}  // namespace hart
