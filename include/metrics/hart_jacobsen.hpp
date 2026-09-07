#pragma once

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

enum class Correction
{
    none,
    candan1,
    candan2
};

}  // namespace Jacobsen

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
            const double candan1Correction = std::tan (piOverN) / (piOverN);  // TODO: Build a cache for common N's
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
