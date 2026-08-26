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

namespace hart
{

/// @brief Estimates the slope of spectral power in log-frequency / log-power space
///
/// The spectrum is divided into logarithmically spaced bands of @p smoothingCents
/// width. Mean power is calculated for each non-empty band, then OLS linear regression
/// is performed on the logarithm of band centre frequency and mean band power.
///
/// @f[
/// \beta =
/// \frac{\sum_i (\log f_i - \overline{\log f})
///                    (\log P_i - \overline{\log P})}
///      {\sum_i (\log f_i - \overline{\log f})^2}
/// @f]
///
/// (beta = sum ((log (f_i) - mean (log(f))) * (log(P_i) - mean (log (P))))
///         / sum ((log (f_i) - mean (log (f))) ** 2)),
///
/// where @f$ f_i @f$ is the geometric centre frequency of band @f$ i @f$ and
/// @f$ P_i @f$ is its mean spectral power.
///
/// For a power-law spectrum
///
/// @f[
/// P(f) \propto f^\beta
/// @f]
///
/// (P (f) is proportional to (f ** beta)),
///
/// the returned unitless slope is the exponent @f$ \beta @f$. Typical values are
/// 0 for white noise, -1 for pink noise, and -2 for brown noise.
///
/// Supports `Unit::none` (as native/default) and `Unit::dB_per_octave` units.
/// When requested as `Unit::dB_per_octave`, the result is converted as
///
/// @f[
/// s_{\mathrm{dB/oct}} = 10 \log_{10}(2)\,\beta
/// @f]
///
/// (s_dB_per_octave = 10 * log10(2) * beta).
///
/// DC is excluded. Only complete logarithmic bands contained inside the selected
/// frequency slice are used.
///
/// @note It is encouraged to be used on a slice of the spectrum, to exclude
/// near-noise and near-Nyquist frequencies for a more accurate estimation:
/// @code
/// const double slope = hart::spectralLogLogSlope (someSpectrum)
///    .at (hart::Slice::freq (20_Hz, 20_kHz))  // Audible range slice
///    .get (hart::mean());
/// @endcode
///
/// @param spectrum Spectrum to analyse.
/// @param smoothingCents Width of each logarithmic averaging band in cents.
/// Must be greater than zero. 1200 cents corresponds to one octave, 100 cents
/// is one semitone.
/// @return A metric query returning the log-log spectral slope, in the requested
/// unit. May return `NaN`.
/// @ingroup Metrics
inline MetricQuery<double> spectralLogLogSlope (const Spectrum& spectrum, double smoothingCents = 1200.0)
{
    if (smoothingCents <= 0.0)
        HART_THROW_OR_RETURN (ValueError, "smoothingCents should be a positive band width", {});

    MetricQuery<double>::SingleChannelMetricEvaluator evaluator =
        [&spectrum, smoothingCents]
        (size_t channel, const Slice& slice, Unit requestedUnit)
        -> double
    {
        hassert (channel < spectrum.getNumChannels());
        hassert (! std::isnan (spectrum.getSampleRateHz()));

        const std::pair<size_t, size_t> binIndices = spectrum.getBinIndices (slice);
        const size_t startBin = std::max<size_t> (1, binIndices.first);
        const size_t stopBin = binIndices.second;

        if (slice.isEmpty() || stopBin - startBin == 0)
            return hart::nan<double>();

        hassert (startBin < stopBin);
        hassert (stopBin <= spectrum.getNumBins());

        struct LogLogPoint
        {
            double logFrequency;
            double logPower;
        };

        std::vector<LogLogPoint> logLogPoints;
        const double bandRatio = centsToRatio (smoothingCents);
        const double stopFrequencyHz = spectrum.getBinFrequencyHz (stopBin - 1);
        double currentBandStartHz = spectrum.getBinFrequencyHz (startBin);
        double currentBandEndHz = currentBandStartHz * bandRatio;

        while (currentBandEndHz <= stopFrequencyHz)
        {
            AccurateSum<double> bandPower;
            size_t numCurrentBandBins = 0;

            for (size_t bin = startBin; bin < stopBin; ++bin)
            {
                const double frequencyHz = spectrum.getBinFrequencyHz (bin);

                if (frequencyHz < currentBandStartHz)
                    continue;

                if (frequencyHz >= currentBandEndHz)
                    break;

                bandPower += std::norm (spectrum.getBinValue (channel, bin));
                ++numCurrentBandBins;
            }

            if (numCurrentBandBins > 0)
            {
                const double binMeanPower = bandPower.getValue() / static_cast<double> (numCurrentBandBins);

                if (floatsEqual (binMeanPower, 0.0))
                    continue;

                // Geometric centre of a logarithmic band
                const double centreFrequencyHz = std::sqrt (currentBandStartHz * currentBandEndHz);

                logLogPoints.push_back ({
                    std::log (centreFrequencyHz),
                    std::log (binMeanPower)
                });
            }

            currentBandStartHz = currentBandEndHz;
            currentBandEndHz = currentBandStartHz * bandRatio;
        }

        // Ordinary least-squares slope
        AccurateSum<double> sumX;
        AccurateSum<double> sumY;

        for (const LogLogPoint logLogPoint : logLogPoints)
        {
            sumX += logLogPoint.logFrequency;
            sumY += logLogPoint.logPower;
        }

        const double meanX = sumX.getValue() / logLogPoints.size();
        const double meanY = sumY.getValue() / logLogPoints.size();

        AccurateSum<double> covariance;
        AccurateSum<double> varianceX;

        for (const LogLogPoint logLogPoint : logLogPoints)
        {
            const double dx = logLogPoint.logFrequency - meanX;
            const double dy = logLogPoint.logPower - meanY;

            covariance += dx * dy;
            varianceX += dx * dx;
        }

        // TODO: Slope can be in db per oct, so add support for dB/oct at some point:
        // slope_db_per_oct = beta * 10 * log10(2) 
        const double slopeUnitless = covariance.getValue() / varianceX.getValue();
        constexpr double threeDb = 3.010299956639812;  // 10 * log10 (2)

        switch (requestedUnit)
        {
            case Unit::native:
            case Unit::none: return slopeUnitless;

            case Unit::dB_per_octave: return slopeUnitless * threeDb;

            default: HART_THROW_OR_RETURN (hart::UnitError, "Unsupported unit",  nan<double>());
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
