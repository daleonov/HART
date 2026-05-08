#pragma once

#include "hart_accurate_sum.hpp"
#include "hart_audio_buffer.hpp"
#include "hart_metrics_common.hpp"  // CorrelationSearchMode

namespace hart
{

/// @brief Calculates maximum normalized cross-correlation between two audio buffers
/// @details
/// Searches for the best normalized cross-correlation value within a specified
/// lag range independently for each selected pair of channels.
///
/// Cross-correlation is calculated using the following formula:
/// @f[
/// \frac{\sum_n x[n]\,y[n+k]}
///      {\sqrt{
///          \left(\sum_n x[n]^2\right)
///          \left(\sum_n y[n+k]^2\right)
///      }}
/// @f]
///
/// (`sum (x[n] * y[n + k]) / sqrt (sum (x[n]^2) * sum (y[n + k]^2))`)
///
/// where:
/// - `x[n]` is the left-hand-side signal
/// - `y[n + k]` is the right-hand-side signal shifted by lag `k`
/// - `k` is searched in the range `[-maxLag, +maxLag]`
///
/// The result is normalized to the range `[-1, 1]`, where:
/// - `+1` means perfect positive correlation
/// - `-1` means perfect negative correlation (polarity inversion)
/// - `0` means no linear correlation
///
/// Depending on @p searchMode, the metric either:
/// - searches for the largest signed correlation value
/// - or searches for the largest absolute correlation value while still returning
///   the original signed correlation.
///
/// Correlation is calculated independently for each selected pair of channels.
/// Use a reducer to combine multiple channel-pair results into a scalar.
///
/// Usage examples:
/// @code
/// // Mono signals, default channel mapping {0,0}
/// const double corr = maxCrossCorrelation (monoInput, monoOutput, 100_ms).get();
///
/// // Same, but polarity-invariant lag search
/// const double corrAbs = maxCrossCorrelation (
///     monoInput,
///     monoOutput,
///     100_ms,
///     bestAbsoluteCorrelation
/// ).get();
///
/// // Stereo signals, strongest matched pair correlation
/// const double maxCorr = maxCrossCorrelation (stereoInput, stereoOutput, 100_ms).get (max());
///
/// // Cross-map channels explicitly
/// const double swappedCorr = maxCrossCorrelation (input, output, 100_ms)
///     .ch ({ {0, 1}, {1, 0} })
///     .get (min());
///
/// // Detect polarity inversion
/// const double corrSigned = maxCrossCorrelation (
///     input,
///     invertedOutput,
///     100_ms,
///     bestAbsoluteCorrelation
/// ).get();
///
/// HART_EXPECT_LT (corrSigned, 0.0);
/// @endcode
///
/// Notes:
/// - Gain differences do not affect the result due to normalization.
/// - DC offset may reduce correlation.
/// - Heavy non-linear processing may significantly reduce correlation.
/// - Returned value remains signed even in `bestAbsoluteCorrelation` mode.
/// - If no valid overlap exists, returns `NaN`.
///
/// Supports only `Unit::native` and `Unit::none` units.
///
/// @param bufferA Left-hand-side audio buffer
/// @param bufferB Right-hand-side audio buffer
/// @param maxLagSeconds Maximum lag to search in seconds
/// @param searchMode Controls how the best lag is selected, see @ref `CorrelationSearchMode`
///
/// @return MetricQuery containing signed normalized cross-correlation values
///
/// @tparam SampleType Floating point sample type, typically `float` or `double`
///
/// @throws hart::ValueError If `maxLagSeconds` is negative
/// @throws hart::SampleRateError If sample rates differ
/// @throws hart::IndexError If requested channel indices are out of range
/// @throws hart::UnitError If unsupported unit is requested
///
/// @ingroup Metrics
template <typename SampleType>
MetricQuery<double> maxCrossCorrelation (
    const AudioBuffer<SampleType>& bufferA,
    const AudioBuffer<SampleType>& bufferB,
    double maxLagSeconds,
    CorrelationSearchMode searchMode = bestAbsoluteCorrelation
)
{
    if (maxLagSeconds < 0.0)
        HART_THROW_OR_RETURN (hart::ValueError,"Maximum lag must be non-negative", {});

    if ((bufferA.hasSampleRate() || bufferB.hasSampleRate()) && bufferA.getSampleRateHz() != bufferB.getSampleRateHz())
        HART_THROW_OR_RETURN (hart::SampleRateError, "Audio buffers must have equal sample rates", {});

    const double sampleRateHz = bufferA.getSampleRateHz();
    const size_t maxLagFrames = static_cast<size_t> (std::round (maxLagSeconds * sampleRateHz));

    typename MetricQuery<double>::ChannelPairMetricEvaluator evaluator =
        [&bufferA, &bufferB, maxLagFrames, searchMode]
        (size_t channelA,size_t channelB, size_t sliceStart, size_t sliceStop, Unit requestedUnit)
        -> double
    {
        // Should be checked by MetricQuery
        hassert (sliceStart < sliceStop);
        hassert (channelA < bufferA.getNumChannels());
        hassert (channelB < bufferB.getNumChannels());

        if (requestedUnit != Unit::native && requestedUnit != Unit::none)
            HART_THROW_OR_RETURN (hart::UnitError, "Cross-correlation does not support requested unit", hart::nan<double>());

        const size_t numFramesA = bufferA.getNumFrames();
        const size_t numFramesB = bufferB.getNumFrames();

        if (sliceStop > numFramesA || sliceStop > numFramesB)
            HART_THROW_OR_RETURN (hart::IndexError, "Slice is out of range", hart::nan<double>());

        const size_t numFrames = sliceStop - sliceStart;

        if (numFrames == 0)
            return hart::nan<double>();

        const SampleType* x = bufferA[channelA] + sliceStart;
        const SampleType* y = bufferB[channelB] + sliceStart;

        double bestCorrelation = (searchMode == bestSignedCorrelation) ? -hart::inf : 0.0;
        bool hadValidOverlap = false;

        for (int lag = -static_cast<int> (maxLagFrames); lag <= static_cast<int> (maxLagFrames); ++lag)
        {
            const bool lagIsNegative = lag < 0;
            const size_t lagAbsFrames = static_cast<size_t> (lagIsNegative ? -lag : lag);

            if (lagAbsFrames >= numFrames)
                continue;

            const size_t xBegin = lagIsNegative ? lagAbsFrames : 0;
            const size_t yBegin = lagIsNegative ? 0 : lagAbsFrames;
            const size_t overlapFrames = numFrames - lagAbsFrames;

            AccurateSum<double> dotProduct;
            AccurateSum<double> sumSquaresX;
            AccurateSum<double> sumSquaresY;

            for (size_t frame = 0; frame < overlapFrames; ++frame)
            {
                const double xn = static_cast<double> (x[xBegin + frame]);
                const double yn = static_cast<double> (y[yBegin + frame]);

                dotProduct += xn * yn;
                sumSquaresX += xn * xn;
                sumSquaresY += yn * yn;
            }

            const double energyX = sumSquaresX.getValue();
            const double energyY = sumSquaresY.getValue();

            if (floatsEqual (energyX, 0.0) || floatsEqual (energyY, 0.0))
                continue;

            hadValidOverlap = true;
            const double correlation = dotProduct.getValue() / std::sqrt (energyX * energyY);

            if (searchMode == bestSignedCorrelation)
            {
                if (correlation > bestCorrelation)
                    bestCorrelation = correlation;
            }
            else bestAbsoluteCorrelation:
            {
                if (std::abs (correlation) > std::abs (bestCorrelation))
                    bestCorrelation = correlation;
            }

            if (floatsEqual (std::abs (bestCorrelation), 1.0))
                break;
        }

        if (! hadValidOverlap)
            return hart::nan<double>();

        return bestCorrelation;
    };

    std::vector<std::pair<size_t, size_t>> defaultChannelPairs;
    const size_t numPairs = std::min (bufferA.getNumChannels(), bufferB.getNumChannels());
    defaultChannelPairs.reserve (numPairs);

    for (size_t channel = 0; channel < numPairs; ++channel)
        defaultChannelPairs.emplace_back (channel, channel);

    return MetricQuery<double> (
        std::move (evaluator),
        bufferA.getNumChannels(),
        bufferB.getNumChannels(),
        std::min (bufferA.getNumFrames(), bufferB.getNumFrames()),
        std::move (defaultChannelPairs)
    );
}

}  // namespace hart
