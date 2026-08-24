#pragma once

#include <algorithm>  // max()
#include <cmath>  // isnan(), sqrt(), log()
#include <complex>  // norm()
#include <utility>  // pair

#include "hart_accurate_sum.hpp"
#include "hart_exceptions.hpp"
#include "metrics/hart_metric_query.hpp"
#include "metrics/hart_metrics_common.hpp"  // ChannelSubsets
#include "hart_slice.hpp"
#include "hart_spectrum.hpp"
#include "hart_units.hpp"  // Unit
#include "hart_utils.hpp"  // nan(), floatsEqual(), centsToRatio()

// TODO: Document it properly

namespace hart
{

/// @brief Calculates difference between two spectra in log-frequency domain
/// @ingroup Metrics
inline MetricQuery<double> logSpectralDistance (const Spectrum& spectrumA, const Spectrum& spectrumB, Normalise normaliseLevels = Normalise::no, double smoothingCents = 100.0)
{
    if (smoothingCents < 0.0 || floatsEqual (smoothingCents, 0.0))
        HART_THROW_OR_RETURN (ValueError, "smoothingCents should be a non-negative band width", {});

    if (floatsNotEqual (spectrumA.getSampleRateHz(), spectrumB.getSampleRateHz()))
        HART_THROW_OR_RETURN (SampleRateError, "Sample rates of the two spectra must match", {});

    if (spectrumA.getFFTSize() != spectrumB.getFFTSize())
        HART_THROW_OR_RETURN (SizeError, "FFT sizes of the two spectra must match", {});

    hassert (spectrumA.getNumBins() == spectrumB.getNumBins());  // If FFT sizes match, so should the numbers of bins

    typename MetricQuery<double>::ChannelPairMetricEvaluator evaluator =
        [&spectrumA, &spectrumB, normaliseLevels, smoothingCents]
        (size_t spectrumAChannel, size_t spectrumBChannel, Slice slice, Unit requestedUnit)
        -> double
    {
        if (requestedUnit != Unit::native && requestedUnit != Unit::dB)
            HART_THROW_OR_RETURN (hart::UnitError, "Unsupported unit", hart::nan<double>());

        hassert (spectrumAChannel < spectrumA.getNumChannels());
        hassert (spectrumBChannel < spectrumB.getNumChannels());
        hassert (! std::isnan (spectrumA.getSampleRateHz()));
        hassert (! std::isnan (spectrumB.getSampleRateHz()));

        const std::pair<size_t, size_t> binIndices = spectrumA.getBinIndices (slice);
        const size_t startBin = std::max<size_t> (1, binIndices.first);
        const size_t stopBin = binIndices.second;

        if (slice.isEmpty() || stopBin - startBin == 0)
            return hart::nan<double>();

        hassert (startBin < stopBin);
        hassert (stopBin <= spectrumA.getNumBins());

        // TODO: Reserve space in those two-vectors
        std::vector<double> levelsADb;
        std::vector<double> levelsBDb;
        AccurateSum<double> levelDifferenceSum;
        const double bandRatio = centsToRatio (smoothingCents);
        const double stopFrequencyHz = spectrumA.getBinFrequencyHz (stopBin - 1);
        double currentBandStartHz = spectrumA.getBinFrequencyHz (startBin);
        double currentBandEndHz = currentBandStartHz * bandRatio;

        while (currentBandEndHz <= stopFrequencyHz)
        {
            AccurateSum<double> bandPowerA;
            AccurateSum<double> bandPowerB;
            size_t numCurrentBandBins = 0;

            for (size_t bin = startBin; bin < stopBin; ++bin)
            {
                const double frequencyHz = spectrumA.getBinFrequencyHz (bin);

                if (frequencyHz < currentBandStartHz)
                    continue;

                if (frequencyHz >= currentBandEndHz)
                    break;

                bandPowerA += std::norm (spectrumA.getBinValue (spectrumAChannel, bin));
                bandPowerB += std::norm (spectrumB.getBinValue (spectrumBChannel, bin));
                ++numCurrentBandBins;
            }

            if (numCurrentBandBins > 0)
            {
                const double binMeanPowerA = bandPowerA.getValue() / static_cast<double> (numCurrentBandBins);
                const double binMeanPowerB = bandPowerB.getValue() / static_cast<double> (numCurrentBandBins);

                const double levelADb = powerToDecibels (binMeanPowerA);
                const double levelBDb = powerToDecibels (binMeanPowerB);

                levelsADb.push_back (levelADb);
                levelsBDb.push_back (levelBDb);

                // For optional gain normalization
                levelDifferenceSum += (levelBDb - levelADb);
            }

            currentBandStartHz = currentBandEndHz;
            currentBandEndHz = currentBandStartHz * bandRatio;
        }

        hassert (levelsADb.size() == levelsBDb.size());
        const size_t numPoints = levelsADb.size();

        if (numPoints == 0)
            return hart::nan<double>();  // At least one of the signals is silent

        const double offsetDb =
            normaliseLevels == Normalise::no
                ? 0.0
                : levelDifferenceSum.getValue() / static_cast<double> (numPoints);

        AccurateSum<double> squaredErrorSum;

        for (size_t i = 0; i < numPoints; ++i)
        {
            const double errorDb = levelsADb[i] - levelsBDb[i] + offsetDb;
            squaredErrorSum += errorDb * errorDb;
        }

        return std::sqrt (squaredErrorSum.getValue() / static_cast<double> (numPoints));
    };

    const size_t numPairs = std::min (spectrumA.getNumChannels(), spectrumB.getNumChannels());
    return MetricQuery<double> (
        std::move (evaluator),
        spectrumA.getNumChannels(),
        spectrumB.getNumChannels(),
        ChannelSubsets::diagonalChannelPairs (numPairs)
    );
}

}  // namespace hart
