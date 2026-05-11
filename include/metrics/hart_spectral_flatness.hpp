#pragma once

#include <cmath>  // log10(), pow()

#include "hart_accurate_sum.hpp"
#include "hart_exceptions.hpp"
#include "metrics/hart_metric_query.hpp"
#include "metrics/hart_metrics_common.hpp"  // ChannelSubsets
#include "hart_slice.hpp"
#include "hart_spectrum.hpp"
#include "hart_units.hpp"  // Unit
#include "hart_utils.hpp"  // nan(), floatsEqual(), powerToDecibels

namespace hart
{

/// @brief Calculates spectral flatness, also known as Wiener entropy, or tonality coefficient.
/// @details Useful to judge how noise-like spectrum is.
///
/// You have an option to pick one of two common weighting methods:
///
/// 1. Magnitude-weighted:
/// @f[
/// \mathrm{SpectralFlatness} =
/// \frac{
///     \exp \left(
///         \frac{1}{N}
///         \sum_{n=0}^{N-1} \ln(x[n])
///     \right)
/// }{
///     \frac{1}{N}
///     \sum_{n=0}^{N-1} x[n]
/// }
/// @f]
///
/// (SpectralFlatness = exp(sum (log (x[n])) / N) / (sum(x[n]) / N),
///
/// Where x[n] is a magnitude of a bin, and N is number of bins.
/// The result can be represented in `Unit::linear` (default/ native) or `Unit::dB`.
/// For decibel value, it will be converted as power (not voltage).
///
/// Typical values:
/// - 0.0 (-oo dB) - highly tonal / sparse spectrum
/// - 1.0 (0 dB) - perfectly flat spectrum
/// - NaN - silence
///
/// See @ref UsingMetricsAndReducers for how to use metrics like this one.
/// @param spectrum Spectrum of a single to operate on
/// @param floorLinear Bin magnitude threshold for numerical stability. Each bin's magnitude will be
/// evaluated as x[n] = max (binMagnitudes[n], floorLinear).
/// @return  Chainable `MetricQuery`, which calculates per-channel spectral flatness values
/// @ingroup Metrics
inline MetricQuery<double> spectralFlatness (const Spectrum& spectrum, double floorLinear = 1e-16)
{
    typename MetricQuery<double>::SingleChannelMetricEvaluator evaluator =
        [&spectrum, floorLinear]
        (size_t channel, const Slice& slice, Unit requestedUnit)
        -> double
    {
        hassert (channel < spectrum.getNumChannels());
        hassert (! std::isnan (spectrum.getSampleRateHz()));

        const std::pair<size_t, size_t> binIndices = spectrum.getBinIndices (slice);
        const size_t startBin = binIndices.first;
        const size_t stopBin = binIndices.second;
        const size_t numBinsInSlice = stopBin - startBin;

        if (slice.isEmpty() || numBinsInSlice == 0)
            return hart::nan<double>();

        hassert (startBin < stopBin);
        hassert (stopBin <= spectrum.getNumBins());

        AccurateSum<double> logSum;
        AccurateSum<double> magnitudeSum;

        for (size_t bin = startBin; bin < stopBin; ++bin)
        {
            const double magnitudeLinear = std::max (spectrum.getBinMagnitude (channel, bin), floorLinear);
            logSum += std::log (magnitudeLinear);
            magnitudeSum += magnitudeLinear;
        }

        if (floatsEqual (magnitudeSum.getValue(), 0.0))
            return hart::nan<double>();

        const double oneOverN = 1.0 / static_cast<double> (numBinsInSlice);
        const double spectralFlatnessLinear = std::exp (oneOverN * logSum.getValue()) / (oneOverN * magnitudeSum.getValue());

        // TODO: Add percent unit, maybe?
        switch (requestedUnit)
        {
            case Unit::native:
            case Unit::linear: return spectralFlatnessLinear;

            case Unit::dB: return hart::powerToDecibels (spectralFlatnessLinear);

            default: HART_THROW_OR_RETURN (hart::UnitError, "Unsupported unit",  hart::nan<double>());
        }
    };

    const size_t numChannels = spectrum.getNumChannels();
    return MetricQuery<double> (
        std::move (evaluator),
        numChannels,
        ChannelSubsets::allChannels (numChannels)
    );
}

}  // namespace hart
