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

namespace RT60
{
    /// @brief RT60 estimation method.
    /// @details
    /// All methods estimate the time required for the impulse response energy
    /// to decay by 60 dB. They differ only in which part of the energy decay
    /// curve is used for the linear fit.
    ///
    /// Used as @ref `rt60()` metric argument.
    enum class Method
    {
        /// Early Decay Time.
        /// Fits the decay from 0 dB to -10 dB and extrapolates the fitted slope
        /// to a 60 dB decay. EDT is influenced primarily by the early part of
        /// the reverberation tail.
        /// See ISO 3382, Annex A for more in-depth description of EDT.
        edt,

        /// T20 reverberation time estimate.
        /// Fits the decay from -5 dB to -25 dB and extrapolates the fitted slope
        /// to a 60 dB decay.
        t20,

        /// T30 reverberation time estimate.
        /// Fits the decay from -5 dB to -35 dB and extrapolates the fitted slope
        /// to a 60 dB decay. T30 uses a larger portion of the decay than T20 and
        /// therefore requires a sufficiently long impulse response.
        t30
    };
}  // namespace RT60

/// @brief Estimates the RT60 reverberation time of an impulse response.
///
/// RT60 is the time required for reverberant energy to decay by 60 dB.
/// The metric is calculated from the impulse response using Schroeder backward
/// integration, followed by linear regression over the range specified by
/// @p method. The fitted decay slope is then extrapolated to 60 dB.
///
/// EDT, T20 and T30, specified by @p method, are different estimators of the
/// same RT60 quantity. For an exponential decay they're expected to produce
/// identical results; for more complex decay curves you can use combination
/// of those.
///
/// Supported units are `Unit::seconds`, `Unit::native` (same as seconds) and
/// `Unit::frames`. If `Unit::frames` is requested, the result will be a fractional
/// value.
///
/// This metric is based on ISO 3382 standard.
/// @note Slices aren't yet supported by this metric, and will be ignored.
/// @attention
/// The supplied impulse response is assumed to represent a decaying response.
/// If the requested decay range cannot be observed, or a valid decay slope
/// cannot be estimated, the result will be `NaN`. Also, note that DSPs that
/// produce no decay or ringing at all (e. g., a system that just applies linear
/// gain, or a stateless waveshaper) will result in `NaN`, and not zero.
///
/// Also, make sure that provided IR is long enough to contain a portion of slope
/// specified by @p method, otherwise the estimation will result `NaN`.
/// See @ref RT60::Method options documentation for details, and ISO 3382 for
/// a more in-depth description.
/// @tparam SampleType Floating-point sample type of the impulse response.
/// @param ir Impulse response to analyze
/// @param method RT60 estimation method
///
/// @return A MetricQuery containing the estimated decay time for each channel.
/// Either in seconds, or in frames, depending on requested unit. Can be `NaN`.
///
/// @see RT60::Method
/// @ingroup Metrics
template <typename SampleType>
MetricQuery<double> rt60 (const ImpulseResponse<SampleType>& ir, RT60::Method method = RT60::Method::edt)
{
    MetricQuery<double>::SingleChannelMetricEvaluator evaluator =
        [&ir, method]
        (size_t channel, Slice /* slice */, Unit requestedUnit)
        -> double
    {
        hassert (channel < ir.getNumChannels());
        const double nan = hart::nan<double>();

        // TODO: Handle Slice for impulse responses in a way that makes sense

        const size_t numFrames = ir.getNumFrames();

        if (numFrames == 0)
            return nan;

        const SampleType* irChannelData = ir[channel];
        
        // Schroeder backward integration
        std::vector<double> energy (numFrames);
        AccurateSum<double> accumulatedEnergy;
    
        for (size_t i = numFrames; i-- > 0;)
        {
            const double sample = static_cast<double> (irChannelData[i]);
            accumulatedEnergy += sample * sample;
            energy[i] = accumulatedEnergy.getValue();
        }

        const auto totalEnergy = energy[0];

        if (totalEnergy <= 0.0)
            return nan;

        const double sampleRateHz = ir.getSampleRateHz();
        hassert (sampleRateHz > 0.0);
        
        struct FitRange
        {
            double upperDb;
            double lowerDb;

            FitRange (RT60::Method method)
            {
                switch (method)
                {
                    case RT60::Method::t20:
                        upperDb = -5.0;
                        lowerDb = -25.0;
                        break;
                    case RT60::Method::t30:
                        upperDb = -5.0;
                        lowerDb = -35.0;
                        break;
                    default:  // Rt60::Method::edt
                        upperDb = 0.0;
                        lowerDb = -10.0;
                }
            }
        };

        const FitRange fitRange (method);

        // Linear regression:
        // y = slope * x + intercept
        // where:
        // x is time in seconds,
        // y is Schroeder energy decay in dB

        AccurateSum<double> sumX;
        AccurateSum<double> sumY;
        AccurateSum<double> sumXX;
        AccurateSum<double> sumXY;
        std::size_t numPoints = 0;
        bool lowerBoundWasReached = false;

        for (std::size_t i = 0; i < numFrames; ++i)
        {
            if (energy[i] <= 0.0)
                break;

            const double decayDb = 10.0 * std::log10 (energy[i] / totalEnergy);

            if (decayDb > fitRange.upperDb)
                continue;

            if (decayDb <= fitRange.lowerDb)
            {
                lowerBoundWasReached = true;
                break;
            }

            const double timeSeconds = static_cast<double> (i) / sampleRateHz;

            sumX += timeSeconds;
            sumY += decayDb;
            sumXX += timeSeconds * timeSeconds;
            sumXY += timeSeconds * decayDb;

            ++numPoints;
        }

        if (! lowerBoundWasReached)
        {
            // Signal (IR) didn't reach the appropriate decay point.
            // Consider using a different RT60::Method, or supply a longer IR.
            return nan;
        }

        if (numPoints < 2)
            return nan;

        const double n = static_cast<double> (numPoints);
        const double denominator = n * sumXX.getValue() - sumX.getValue() * sumX.getValue();

        if (floatsEqual (denominator, 0.0))
            return nan;

        const auto slopeDbPerSecond = (n * sumXY.getValue() - sumX.getValue() * sumY.getValue()) / denominator;
        hassert (slopeDbPerSecond < 0.0);  // A valid Schroeder decay fit must have a negative slope

        const double sixtyDbDecayTimeSeconds = -60.0 / slopeDbPerSecond;

        switch (requestedUnit)
        {
            case Unit::native:
            case Unit::seconds: return sixtyDbDecayTimeSeconds;

            case Unit::frames: return sixtyDbDecayTimeSeconds * sampleRateHz;

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
