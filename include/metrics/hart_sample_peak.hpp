#pragma once

#include <algorithm>  // max()
#include <cmath>  // abs()

#include "hart_audio_buffer.hpp"
#include "hart_exceptions.hpp"
#include "hart_metric_query.hpp"
#include "hart_utils.hpp"  // nan

namespace hart
{

/// @brief Calculates Sample Peak of an audio buffer
/// @details Calculates rectified peak values for each channel, use a reducer to
/// get a scalar value (see @ref Reducers). Supports `Unit::linear` (default) and
/// `Unit::dB` units. Usage example:
/// @code
/// HART_EXPECT_FLOAT_EQ (samplePeak (monoBuffer).as (dB).get(), -3_dB, 1e-2);
/// HART_EXPECT_LT (samplePeak (monoBuffer).as (linear).get(), 1.0, 1e-3);
/// HART_EXPECT_LT (samplePeak (stereoBuffer).as (dB).get (max()), -3_dB);
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
    typename MetricQuery<SampleType>::MetricEvaluator evaluator =
        [&audioBuffer]
        (size_t channel, size_t sliceStart, size_t sliceStop, Unit requestedUnit)
        -> double
    {
        hassert (sliceStart < sliceStop);  // Should be handled by MetricQuery

        const size_t numFrames = audioBuffer.getNumFrames();

        if (sliceStart >= numFrames)
            HART_THROW_OR_RETURN (hart::IndexError, "Slice start is out of range", hart::nan<double>());

        if (sliceStop > numFrames)
            HART_THROW_OR_RETURN (hart::IndexError, "Slice end is out of range", hart::nan<double>());

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

    return MetricQuery<double> (
        std::move (evaluator),
        audioBuffer.getNumChannels(),
        audioBuffer.getNumFrames()
    );
}

}  // namespace hart
