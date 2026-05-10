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

namespace SpectralCentroid
{
    enum class Weighting
    {
        magnitude,
        power
    };

}  // namespace SpectralCentroid

/// @brief Calculates spectral centroid
/// @details Commonly used to numerically express amound of "brightness" of a sound.
///
/// You have an option to pick one of two common weighting methods:
///
/// 1. Magnitude-weighted:
/// @f[
/// C_{mag}=\frac{\sum_{k=0}^{N-1} f_k |X_k|}{\sum_{k=0}^{N-1} |X_k|}
/// @f]
///
/// (C_mag = sum(f_k * abs(X_k)) / sum(abs(X_k))),
///
/// 2. Power-weighted:
/// @f[
/// C_{pow}=\frac{\sum_{k=0}^{N-1} f_k |X_k|^2}{\sum_{k=0}^{N-1} |X_k|^2}
/// @f]
///
/// C_pow = sum(f_k * abs(X_k)^2) / sum(abs(X_k)^2)
///
/// Where f<sub>k</sub> is a center frequency of each bin, and X<sub>k</sub> is a magnitude of this bin.
/// In both cases the result is measured in Hertz, so accepted units are Unit::native and Unit::Hz,
/// which are the same.
///
/// See @ref UsingMetricsAndReducers for how to use metrics that return a `MetricQuery` object line this one.
/// @param spectrum Spectrum of a single to operate on
/// @param weighting Type of weighting (see description above)
/// @return  Chainable `MetricQuery`, which calculates per-channel spectral centroid values in Hz
/// @ingroup Metrics
inline MetricQuery<double> spectralCentroid (const Spectrum& spectrum, SpectralCentroid::Weighting weighting = SpectralCentroid::Weighting::magnitude)
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

        if (SpectralCentroid::Weighting = SpectralCentroid::Weighting::magnitude)
        {
            for (size_t bin = startBin; bin < stopBin; ++bin)
            {
                const double magnitudeLinear = spectrum.getBinMagnitude (channel, bin);
                const double frequencyHz = spectrum.getBinFrequencyHz (bin);

                numerator += magnitudeLinear * frequencyHz;
                denominator += magnitudeLinear;
            }
        }
        else  // SpectralCentroid::Weighting = SpectralCentroid::Weighting::power
        {
            for (size_t bin = startBin; bin < stopBin; ++bin)
            {
                const double magnitudeLinear = spectrum.getBinMagnitude (channel, bin);
                const double powerLinear = magnitudeLinear * magnitudeLinear;
                const double frequencyHz = spectrum.getBinFrequencyHz (bin);

                numerator += powerLinear * frequencyHz;
                denominator += powerLinear;
            }
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
