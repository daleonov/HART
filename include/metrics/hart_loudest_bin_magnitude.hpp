#pragma once

#include <algorithm>  // max()
#include <vector>

#include "hart_exceptions.hpp"
#include "metrics/hart_metric_query.hpp"
#include "hart_slice.hpp"
#include "hart_spectrum.hpp"
#include "hart_units.hpp"  // Unit
#include "hart_utils.hpp"  // nan()

namespace hart
{

/// @brief Calculates the magnitude of the loudest FFT bin
/// @details
/// Finds the maximum-magnitude FFT bin independently for each channel.
/// Use reducers to combine multi-channel results (see @ref Reducers).
///
/// Supports:
/// - `Unit::linear` (native/default)
/// - `Unit::dB` as linear ratio, not power
///
/// This metric operates on FFT magnitudes exactly as stored in the Spectrum.
///
/// Typical use cases:
/// - alias detection
/// - spur detection
/// - harmonic inspection
/// - FFT sanity checks
/// - spectral leakage diagnostics
///
/// Usage examples:
/// @code
/// // Loudest spectral component in dB
/// const double loudestDb = loudestBinMagnitude (spectrum).as (dB).get();
///
/// // Loudest bin only inside a frequency range
/// const double loudestMidBandDb =
///     loudestBinMagnitude (spectrum)
///         .as (dB)
///         .at (Slice::frequency (500_Hz, 2000_Hz))
///         .get();
///
/// // Loudest bin, but only inside a specific channel subset, as a
/// // linear value (not dB). Calculated per chanel, then averaged.
/// const double loudestBinLinear =
///     loudestBinMagnitude (spectrum)
///         .as (linear)
///         .ch ({0, 2, 4})
///         .get (mean());
///
/// @endcode
///
/// @param spectrum Input frequency-domain spectrum
/// @throws hart::UnitError if unsupported unit is requested
/// @ingroup Metrics
inline MetricQuery<double> loudestBinMagnitude (const Spectrum& spectrum)
{
    typename MetricQuery<double>::SingleChannelMetricEvaluator evaluator =
        [&spectrum]
        (size_t channel, const Slice& slice, Unit requestedUnit)
        -> double
    {
        hassert (channel < spectrum.getNumChannels());

        const std::pair<size_t, size_t> binIndices = spectrum.getBinIndices (slice);
        const size_t startBin = binIndices.first;
        const size_t stopBin = binIndices.second;

        if (slice.isEmpty() || stopBin - startBin == 0)
            return hart::nan<double>();

        hassert (startBin < stopBin);
        hassert (stopBin <= spectrum.getNumBins());

        const double* bins = spectrum[channel];
        double maxMagnitude = 0.0;

        for (size_t bin = startBin; bin < stopBin; ++bin)
            maxMagnitude = std::max (bins[bin], maxMagnitude);

        switch (requestedUnit)
        {
            case Unit::native:
            case Unit::linear:
            {
                return maxMagnitude;
            }

            case Unit::dB:
            {
                return hart::ratioToDecibels (maxMagnitude);
            }

            default:
            {
                HART_THROW_OR_RETURN (hart::UnitError, "Unsupported unit", hart::nan<double>());
            }
        }
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
