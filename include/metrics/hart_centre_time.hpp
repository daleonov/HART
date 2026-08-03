#pragma once

#include <vector>

#include "hart_accurate_sum.hpp"
#include "hart_impulse_response.hpp"
#include "hart_exceptions.hpp"
#include "metrics/hart_metric_query.hpp"
#include "metrics/hart_metrics_common.hpp"  // ChannelSubsets
#include "hart_slice.hpp"
#include "hart_units.hpp"  // Unit
#include "hart_utils.hpp"  // nan(), floatsEqual()

namespace hart
{

/// @brief Calculates the centre time of an impulse response.
///
/// Centre time (center time) is the energy-weighted mean arrival time of the
/// impulse response. It describes where the energy of the response is centred
/// in time: responses with more energy occurring later have a larger centre time.
/// Can be used to verify shape of the decay, in combination with `hart::rt60()`.
///
/// Calculated as:
///
/// @f[
/// T_s=\frac{\sum_{n=0}^{N-1} t_n h[n]^2}
///           {\sum_{n=0}^{N-1} h[n]^2}
/// @f]
///
/// (T_s = sum(t_n * h[n]^2) / sum(h[n]^2)),
///
/// where @f$h[n]@f$ is the impulse response sample, and @f$t_n@f$ is its
/// time position (offset) in seconds.
///
/// Supported units are `Unit::seconds`, `Unit::native` (same as seconds) and
/// `Unit::frames`. If `Unit::frames` is requested, the result will be a fractional
/// value.
///
/// The result is NaN if the impulse response contains no energy.
///
/// @tparam SampleType Floating-point sample type of the impulse response.
/// @param ir Impulse response to analyse.
///
/// @return A MetricQuery containing the centre time for each channel.
/// Either in seconds, or in frames, depending on requested unit.
/// May return `NaN`.
/// @ingroup Metrics
template <typename SampleType>
MetricQuery<double> centreTime (const ImpulseResponse<SampleType>& ir)
{
    MetricQuery<double>::SingleChannelMetricEvaluator evaluator =
        [&ir]
        (size_t channel, Slice /* slice */, Unit requestedUnit)
        -> double
    {
        hassert (channel < ir.getNumChannels());
        const double nan = hart::nan<double>();

        // TODO: Handle Slice for impulse responses in a way that makes sense

        const size_t numFrames = ir.getNumFrames();

        if (numFrames == 0)
            return nan;

        const double sampleRateHz = ir.getSampleRateHz();
        hassert (sampleRateHz > 0.0);
        const double samplePeriodSeconds = 1.0 / sampleRateHz;

        const SampleType* irChannelData = ir[channel];
        
        AccurateSum<double> numerator;
        AccurateSum<double> denominator;

        for (size_t i = 0; i < numFrames; ++i)
        {
            const double h = static_cast<double> (irChannelData[i]);
            const double hSquared = h * h;
            const double t = i * samplePeriodSeconds;

            numerator += t * hSquared;
            denominator += hSquared;
        }

        if (floatsEqual (denominator.getValue(), 0.0))
            return nan;

        const double centreTimeSeconds = numerator.getValue() / denominator.getValue();

        switch (requestedUnit)
        {
            case Unit::native:
            case Unit::seconds: return centreTimeSeconds;

            case Unit::frames: return centreTimeSeconds * sampleRateHz;

            default: HART_THROW_OR_RETURN (hart::UnitError, "Unsupported unit",  hart::nan<double>());
        }
    };

    const size_t numChannels = ir.getNumChannels();
    return MetricQuery<double> (
        std::move (evaluator),
        numChannels,
        ChannelSubsets::allChannels (numChannels)
    );
}

}  // namespace hart
