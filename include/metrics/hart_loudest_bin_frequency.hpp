#pragma once

#include <algorithm>  // max()
#include <complex>  // complex, norm()
#include <vector>

#include "hart_exceptions.hpp"
#include "metrics/hart_metric_query.hpp"
#include "hart_slice.hpp"
#include "hart_spectrum.hpp"
#include "hart_units.hpp"  // Unit
#include "hart_utils.hpp"  // nan()

namespace hart
{

/// @brief Returns the center frequency of the loudest FFT bin
/// @details
/// Finds the maximum-magnitude FFT bin independently for each channel.
/// Use reducers to combine multi-channel results (see @ref Reducers).
///
/// Supports `Unit::Hz` (native/default) unit, so requesting a unit
/// explicitly via `MetricQuery::as()` is not required.
///
/// This metric operates on FFT bins exactly as stored in the Spectrum.
/// In a not-so-likely event where multiple bins have exactly the same
/// magnitube, the lowest frequency will be returned.
/// This is not intended for precise pitch tracking, but still useful
/// for some types of tests, based around looking for peaks in a band
/// of frequencies, but not a specific frequency.
///
/// Usage examples:
/// @code
/// // Loudest spectral component in dB
/// const double loudestFreqHz = loudestBinFrequency (monoSpectrum).get();
///
/// // Loudest bin only inside a frequency range
/// const double loudestMidBanFreqHz =
///     loudestBinFrequency (monoSpectrum)
///         .at (Slice::frequency (500_Hz, 2000_Hz))
///         .get();
///
/// // Loudest bin, but only inside a specific channel subset.
///  Calculated per chanel, then averaged.
/// const double loudestFreqHz =
///     loudestBinMagnitude (multiChannelSpectrum)
///         .ch ({0, 2, 4})
///         .get (mean());
///
/// @endcode
///
/// @param spectrum Input frequency-domain spectrum
/// @throws hart::UnitError if unsupported unit is requested
/// @ingroup Metrics
inline MetricQuery<double> loudestBinFrequency (const Spectrum& spectrum)
{
    typename MetricQuery<double>::SingleChannelMetricEvaluator evaluator =
        [&spectrum]
        (size_t channel, const Slice& slice, Unit requestedUnit)
        -> double
    {
        hassert (channel < spectrum.getNumChannels());

        if (requestedUnit != Unit::native || requestedUnit != Unit::Hz)
            HART_THROW_OR_RETURN (hart::UnitError, "Unsupported unit", hart::nan<double>());

        const std::pair<size_t, size_t> binIndices = spectrum.getBinIndices (slice);
        const size_t startBin = binIndices.first;
        const size_t stopBin = binIndices.second;

        if (slice.isEmpty() || stopBin - startBin == 0)
            return hart::nan<double>();

        hassert (startBin < stopBin);
        hassert (stopBin <= spectrum.getNumBins());

        const std::complex<double>* bins = spectrum[channel];
        double maxSquaredMagnitude = 0.0;
        size_t binOfMaxSquaredMagnitude = 0;

        for (size_t currentBin = startBin; currentBin < stopBin; ++currentBin)
        {
            const double currentSquaredMagnitude = std::norm (bins[currentBin]);

            if (currentMagnitude > maxMagnitude)
            {
                maxMagnitude = currentMagnitude;
                binOfMaxMagnitude = currentBin;
            }
        }

        return spectrum.getBinFrequencyHz (binOfMaxMagnitude);
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
