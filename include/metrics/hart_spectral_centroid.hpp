#pragma once

#include <vector>

#include "hart_accurate_sum.hpp"
#include "hart_exceptions.hpp"
#include "metrics/hart_metric_query.hpp"
#include "hart_slice.hpp"
#include "hart_spectrum.hpp"
#include "hart_units.hpp"  // Unit
#include "hart_utils.hpp"  // nan(), floatsEqual()

// TODO: Add more detailed description

namespace hart
{

/// @brief Calculates spectral centroid
/// @ingroup Metrics
inline MetricQuery<double> spectralCentroid (const Spectrum& spectrum)
{
    typename MetricQuery<double>::SingleChannelMetricEvaluator evaluator =
        [&spectrum]
        (size_t channel, const Slice& slice, Unit requestedUnit)
        -> double
    {
        hassert (channel < spectrum.getNumChannels());
        hassert (! std::isnan (spectrum.getSampleRateHz()));

        if (requestedUnit != Unit::native && requestedUnit != Unit::Hz)
            HART_THROW_OR_RETURN (hart::UnitError, "Unsupported unit", hart::nan<double>());

        const std::pair<size_t, size_t> binIndices = spectrum.getBinIndices (slice);
        const size_t startBin = binIndices.first;
        const size_t stopBin = binIndices.second;

        if (slice.isEmpty() || stopBin - startBin == 0)
            return hart::nan<double>();

        hassert (startBin < stopBin);
        hassert (stopBin <= spectrum.getNumBins());

        AccurateSum<double> numerator;
        AccurateSum<double> denominator;

        for (size_t bin = startBin; bin < stopBin; ++bin)
        {
            const double magnitudeLinear = spectrum.getBinMagnitude (channel, bin);
            const double frequencyHz = spectrum.getBinFrequencyHz (bin);

            numerator += magnitudeLinear * frequencyHz;
            denominator += magnitudeLinear;
        }

        if (floatsEqual (denominator.getValue(), 0.0))
            return hart::nan<double>();

        return numerator.getValue() / denominator.getValue();
    };

    std::vector<size_t> defaultChannelsToProcess (
        spectrum.getNumChannels());

    for (size_t i = 0; i < defaultChannelsToProcess.size(); ++i)
        defaultChannelsToProcess[i] = i;

    return MetricQuery<double> (
        std::move (evaluator),
        spectrum.getNumChannels(),
        std::move (defaultChannelsToProcess)
    );
}

}  // namespace hart
