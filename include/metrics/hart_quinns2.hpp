#pragma once

#include <algorithm>  // max()
#include <cmath>  // log(), sqrt(), isnan()
#include <complex>  // complex, norm()

#include "hart_exceptions.hpp"
#include "metrics/hart_metric_query.hpp"
#include "metrics/hart_metrics_common.hpp"  // ChannelSubsets
#include "hart_slice.hpp"
#include "hart_spectrum.hpp"
#include "hart_units.hpp"  // Unit
#include "hart_utils.hpp"  // nan(), floatsEqual()

namespace hart
{

/// @brief Returns somewhat accurate loudest frequency in the spectrum
/// @details
/// Implements algorithm commonly referred to as "Quinn's Second Estimator",
/// described by B. G. Quinn in "Estimating frequency by interpolation
/// using Fourier coefficients", IEEE Transactions on Signal Processing,
/// Vol. 42, No. 5, pp. 1264-1268.
///
/// It's provides a quite accurate way if interpolating frequency value,
/// that is somewhere in between FFT bin centers. Note that it's undefined
/// near DC and Nyquist frequencies, and it will return NaN for those bins.
/// For those edge cases, consider using a more simple
/// `hart::loudestBinFrequency()` metric instead.
///
/// Supports `Unit::Hz` (native/default) unit, so requesting a unit
/// explicitly via `MetricQuery::as()` is not required.
///
/// This metric operates on FFT bins exactly as stored in the Spectrum.
/// In a not-so-likely event where multiple bins have exactly the same
/// magnitube, the lowest frequency will be returned.
/// 
/// Usage examples:
/// @code
/// // Estimated loudest frequency in Hz
/// const double loudestFreqHz = quinns2 (monoSpectrum).get();
///
/// // Estimated loudest frequency inside a frequency band
/// const double loudestMidBandFreqHz =
///     quinns2 (monoSpectrum)
///         .at (Slice::frequency (500_Hz, 2000_Hz))
///         .get();
///
/// // Loudest frequency, but only inside a specific channel subset.
///  Calculated per chanel, then averaged.
/// const double loudestFreqHz =
///     quinns2 (multiChannelSpectrum)
///         .ch ({0, 2, 4})
///         .get (mean());
///
/// @endcode
///
/// @param spectrum Input frequency-domain spectrum
/// @throws hart::UnitError if unsupported unit is requested
/// @ingroup Metrics
inline MetricQuery<double> quinns2 (const Spectrum& spectrum)
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

        if (floatsEqual (spectrum.getSampleRateHz(), 0.0))
            HART_THROW_OR_RETURN (hart::SampleRateError, "Sample rate of spectrum must not be zero", hart::nan<double>());

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

            if (currentSquaredMagnitude > maxSquaredMagnitude)
            {
                maxSquaredMagnitude = currentSquaredMagnitude;
                binOfMaxSquaredMagnitude = currentBin;
            }
        }

        // Quinn's 2nd estimate doesn't seem to be defined for boundary bins
        if (binOfMaxSquaredMagnitude == 0 || binOfMaxSquaredMagnitude == spectrum.getNumBins() - 1)
            return hart::nan<double>();

        // Euclidian norm of max value is to be used as a denominator, so undefines if zero
        if (floatsEqual (maxSquaredMagnitude, 0.0))
            return hart::nan<double>();

        // Based on this implementation:
        // https://gist.github.com/hiromorozumi/f74fd4d5592a7f79028560cb2922d05f

        // Helper function used for frequency estimation
        const auto tau = [] (double x) -> double
        {
            constexpr double sqrtTwoThirds = 0.816496580927726;  // sqrt (2.0 / 3.0)
            constexpr double sqrtSixBy24 = 0.10206207261596574;  // sqrt (6.0) / 24.0

            const double p1 = std::log (3 * x * x + 6 * x + 1);
            const double part1 = x + 1 - sqrtTwoThirds;
            const double part2 = x + 1 + sqrtTwoThirds;
            const double p2 = std::log (part1 / part2);

            return (0.25 * p1 - sqrtSixBy24 * p2);
        };

        const size_t k = binOfMaxSquaredMagnitude;
        const double n = static_cast<double> (spectrum.getFFTSize());
        const double sampleRateHz = spectrum.getSampleRateHz();

        const double divider = std::norm (bins[k]);
        const double ap = (bins[k + 1].real() * bins[k].real() + bins[k + 1].imag() * bins[k].imag()) / divider;
        const double dp = -ap / (1.0 - ap);
        const double am = (bins[k-1].real() * bins[k].real() + bins[k-1].imag() * bins[k].imag()) / divider;
        const double dm = am / (1.0 - am);
        const double d = 0.5 * (dp + dm) + tau (dp * dp) - tau (dm * dm);

        const double adjustedBinLocation = static_cast<double> (k) + d;
        const double peakFreqAdjusted = (sampleRateHz * adjustedBinLocation / n);

        return peakFreqAdjusted;
    };

    const size_t numChannels = spectrum.getNumChannels();
    return MetricQuery<double> (
        std::move (evaluator),
        numChannels,
        ChannelSubsets::allChannels (numChannels)
    );
}

}  // namespace hart
