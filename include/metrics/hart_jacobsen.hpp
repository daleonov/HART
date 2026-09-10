#pragma once

#include <array>
#include <cmath>  // isnan(), tan(), atan()
#include <complex>  // complex, norm(), real()

#include "hart_exceptions.hpp"
#include "metrics/hart_metric_query.hpp"
#include "metrics/hart_metrics_common.hpp"  // ChannelSubsets
#include "hart_slice.hpp"
#include "hart_spectrum.hpp"
#include "hart_units.hpp"  // Unit
#include "hart_utils.hpp"  // nan(), floatIsZero(), complexIsZero(), isPowerOfTwo()

// TODO: Document it

namespace hart
{

namespace Jacobsen
{

/// @brief Selects an optional bias correction for Jacobsen's three-bin frequency estimator.
enum class Correction
{
    /// @brief The original Jacobsen estimator. The fastest of the lot.
    /// @details @see Jacobsen E., Kootsookos P.,
    /// "Fast, Accurate Frequency Estimators",
    /// IEEE Signal Processing Magazine, Vol. 24, Issue 3, pp. 123-125, May, 2007.
    none,

    /// @brief First Candan correction
    /// @details A correction proposed by Cagatay Candan to inprove the accuracy of
    /// frequency estimation. Under the hood, it uses pre-calculated values, and
    /// comes at a cost of just one multiplication operation, as compared to "vanilla"
    /// Jacobsen's estimator (as in `Correction::none`).
    /// @see C. Candan,
    /// "A Method for Fine Resolution Frequency Estimation From Three DFT Samples",
    /// IEEE Signal Processing Letters, Vol. 18, No. 6, pp. 351-354, June 2011.
    candan1,

    /// @brief Second Candan correction
    /// @details Additionally applies the bias-removal refinement. Significantly more
    /// costly than other two options, due to runtime `atan()` calculation, but the
    /// most precise of the lot.
    ///  @see C. Candan,
    /// "Analysis and Further Improvement of Fine Resolution Frequency Estimation
    /// Method From Three DFT Samples",
    /// IEEE Signal Processing Letters, vol. 20, No. 9, pp. 913-916, September, 2013.
    candan2
};

/// @brief Pre-calculated values for Candan's 1st correction
/// @private
constexpr std::array<double, 28> candan1Corrections =
{{
    nan<double>(),  // N = 1 (2 ** 0) - Too small for this three-bin estimator
    nan<double>(),  // N = 2 (2 ** 1) - Too small for this three-bin estimator
    1.2732395447351625,  // N = 4 (2 ** 2)
    1.0547861751580989,  // N = 8 (2 ** 3)
    1.0130523683386767,  // N = 16 (2 ** 4)
    1.0032251965664247,  // N = 32 (2 ** 5)
    1.0008039653559875,  // N = 64 (2 ** 6)
    1.0002008460110010,  // N = 128 (2 ** 7)
    1.0000502024280562,  // N = 256 (2 ** 8)
    1.0000125500399752,  // N = 512 (2 ** 9)
    1.0000031374745559,  // N = 1024 (2 ** 10)
    1.0000007843664240,  // N = 2048 (2 ** 11)
    1.0000001960914675,  // N = 4096 (2 ** 12)
    1.0000000490228582,  // N = 8192 (2 ** 13)
    1.0000000122557140,  // N = 16384 (2 ** 14)
    1.0000000030639284,  // N = 32768 (2 ** 15)
    1.0000000007659822,  // N = 65536 (2 ** 16)
    1.0000000001914955,  // N = 131072 (2 ** 17)
    1.0000000000478739,  // N = 262144 (2 ** 18)
    1.0000000000119684,  // N = 524288 (2 ** 19)
    1.0000000000029921,  // N = 1048576 (2 ** 20)
    1.0000000000007481,  // N = 2097152 (2 ** 21)
    1.0000000000001870,  // N = 4194304 (2 ** 22)
    1.0000000000000469,  // N = 8388608 (2 ** 23)
    1.0000000000000118,  // N = 16777216 (2 ** 24)
    1.0000000000000029,  // N = 33554432 (2 ** 25)
    1.0000000000000007,  // N = 67108864 (2 ** 26)
    1.0000000000000002  // N = 134217728 (2 ** 27)
}};

/// @brief Returns a Candan's 1st correction for the Jacobsen estimator
/// @private 
static inline double getCandan1Correction (size_t fftSize)
{
    if (! isPowerOfTwo (fftSize))
    {
        // Technically, it's still correct, but HART's
        // spectra are power-of-two sized, and expected
        // to use cached values. For arbitrary sizes,
        // we'll  have to calculate it in runtime.
        hassertfalse;
    
        const double n = static_cast<double> (fftSize);
        const double piOverN = pi / n;
        return std::tan (piOverN) / (piOverN);
    }

    if (fftSize > 134217728ull)  // 2 ** 27
        return 1.0;

    const size_t i = integerLog2 (fftSize);
    hassert (i < candan1Corrections.size());

    return candan1Corrections[i];
}

}  // namespace Jacobsen


/// @brief Estimates the frequency of a spectral peak using Jacobsen's three-bin frequency estimator.
///
/// @details
/// Finds the bin with the greatest magnitude within the requested spectrum
/// slice and refines its frequency estimate using that bin and its two
/// neighbouring DFT bins.
///
/// The optional @p correction argument can be used to apply either of
/// Candan's refinements to the original Jacobsen estimator. By default,
/// no correction is applied.
///
/// The estimate is not defined when the detected peak is at a boundary of
/// the available spectrum, since both neighbouring DFT bins are required.
///
/// This is a spectral peak-frequency estimator rather than a general
/// fundamental-frequency detector. For signals containing multiple spectral
/// components, the estimated frequency corresponds to the strongest peak in
/// the selected spectrum slice
/// (via `jacobsen (spectrum).at (Slice (startHz, stopHz))`).
///
/// Supports `Unit::Hz`, this is the only supported unit, and a default (native) one.
///
/// For a detailed comparison of this method and Candan's corrections, see
/// [A Brief Examination of Current and a Proposed Fine Frequency Estimator Using Three DFT Samples](https://www.ericjacobsen.org/Files/Jacobsen_2015_estimator_comparison.pdf)
/// by Eric Jacobsen, as well as the original papers, referenced in it.
///
/// For other accurate peak frequency estimations, you may also check Quinn's
/// second estimator metric: `quinns2()`.
///
/// @param spectrum Spectrum to analyse.
/// @param correction Optional correction applied to the original Jacobsen
/// estimate. Defaults to Jacobsen::Correction::none.
/// @return Chainable `MetricQuery` object, calculates per-channel estimated
/// peak frequencies in Hz.
/// @ingroup Metrics
inline MetricQuery<double> jacobsen (const Spectrum& spectrum, Jacobsen::Correction correction = Jacobsen::Correction::none)
{
    typename MetricQuery<double>::SingleChannelMetricEvaluator evaluator =
        [&spectrum, correction]
        (size_t channel, const Slice& slice, Unit requestedUnit)
        -> double
    {
        hassert (channel < spectrum.getNumChannels());
        hassert (! std::isnan (spectrum.getSampleRateHz()));

        if (requestedUnit != Unit::native && requestedUnit != Unit::Hz)
            HART_THROW_OR_RETURN (hart::UnitError, "Unsupported unit", hart::nan<double>());

        const double sampleRateHz = spectrum.getSampleRateHz();

        if (floatIsZero (sampleRateHz))
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

        // This estimate isn't be defined for boundary bins
        if (binOfMaxSquaredMagnitude == 0 || binOfMaxSquaredMagnitude == spectrum.getNumBins() - 1)
            return hart::nan<double>();

        const size_t k = binOfMaxSquaredMagnitude;
        const std::complex<double> numerator = bins[k - 1] - bins[k + 1];
        const std::complex<double> denominator = bins[k] + bins[k] - bins[k - 1] - bins[k + 1];

        if (complexIsZero (denominator))
            return hart::nan<double>();

        const double deltaJacobsen = std::real (numerator / denominator);
        double delta = deltaJacobsen;

        if (correction == Jacobsen::Correction::candan1 || correction == Jacobsen::Correction::candan2)
        {
            const size_t nOneSided = spectrum.getNumBins();
            const size_t nTwoSided = 2 * (nOneSided - 1);

            // Jacobsen's estimator and both Candan's corrections work for any number
            // of bins, but we expect Spectrum to be a power-of-two-sized FFT
            hassert (isPowerOfTwo (nTwoSided));

            const double n = static_cast<double> (nTwoSided);
            const double piOverN = pi / n;
            const double candan1Correction = Jacobsen::getCandan1Correction (nTwoSided);
            const double deltaCandan1 = candan1Correction * deltaJacobsen;

            delta =  correction == Jacobsen::Correction::candan2
                ? std::atan (deltaCandan1 * piOverN) / piOverN  // Candan's second
                : deltaCandan1;  // Candan's first
        }

        const double binWidthHz = spectrum.getBinWidthHz();
        const double estimatedPeakFrequencyHz = (static_cast<double> (k) + delta) * binWidthHz;
        return estimatedPeakFrequencyHz;
    };

    const size_t numChannels = spectrum.getNumChannels();
    return MetricQuery<double> (
        std::move (evaluator),
        numChannels,
        ChannelSubsets::allChannels (numChannels)
    );
}

}  // namespace hart
