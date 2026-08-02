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

// TODO: Document it!

namespace hart
{

namespace RT60
{
    enum class Method
    {
        edt,
        t20,
        t30
    };
}  // namespace RT60

/// @brief 
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
            hassertfalse;
            return nan;
        }

        if (numPoints < 2)
            return nan;

        const double n = static_cast<double> (numPoints);
        const double denominator = n * sumXX.getValue() - sumX.getValue() * sumX.getValue();

        if (floatsEqual (denominator, 0.0))
            return nan;

        const auto slopeDbPerSecond = (n * sumXY.getValue() - sumX.getValue() * sumY.getValue()) / denominator;

        // Signal gets louder, instead of decaying
        if (slopeDbPerSecond >= 0.0)
            return nan;

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
