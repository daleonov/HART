#pragma once

#include <algorithm>  // min()
#include <cmath>  // abs(), sqrt()

#include "hart_accurate_sum.hpp"
#include "hart_audio_buffer.hpp"
#include "hart_exceptions.hpp"
#include "metrics/hart_metric_query.hpp"
#include "metrics/hart_metrics_common.hpp"  // CorrelationSearchMode, ChannelSubsets
#include "hart_slice.hpp"
#include "hart_units.hpp"  // Unit
#include "hart_utils.hpp"  // floatsEqual()

namespace hart
{

/// @brief Calculates lag corresponding to maximum normalized cross-correlation between two audio buffers
/// @details
/// Searches for the lag producing the strongest normalized cross-correlation
/// independently for each selected pair of channels.
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
/// Positive lag means that `bufferB` is delayed relative to `bufferA`.
///
/// Depending on `searchMode`, the metric either:
/// - searches for the largest signed correlation value
/// - or searches for the largest absolute correlation value
///
/// Correlation is calculated independently for each selected pair of channels.
/// Use a reducer to combine multiple lag values into a scalar.
///
/// Supports `Unit::frames` (default/native) and `Unit::seconds`.
/// For conversion to seconds, it uses sample rate metadata contained
/// in the provided buffers.
///
/// Usage examples:
/// @code
/// // Detect latency in frames
/// const double lagFrames = lagAtMaxCrossCorrelation (input, output, 100_ms).get();
///
/// // Same, but returned in seconds
/// const double lagSeconds = lagAtMaxCrossCorrelation (input, output, 100_ms)
///     .as (seconds)
///     .get();
///
/// // Strongest lag across matched stereo channels
/// const double maxLagFrames = lagAtMaxCrossCorrelation (input, output, 100_ms)
///     .get (max());
///
/// // Custom channel mapping
/// // Lags between:
/// //  - input, channel 0 vs output, channel 1
/// //  - input, channel 3 vs output, channel 3
/// //  - input, channel 1 vs output, channel 2
/// const double swappedLagFrames = lagAtMaxCrossCorrelation (multiChanneInput, multiChanneOutput, 100_ms)
///     .ch ({{0, 1}, {3, 3}, {1, 2}})
///     .get (mean());
/// @endcode
///
/// Notes:
/// - Gain differences do not affect the result due to normalization.
/// - Returned lag may be negative.
/// - If no valid overlap exists, returns `NaN`.
///
/// @param bufferA Left-hand-side audio buffer
/// @param bufferB Right-hand-side audio buffer
/// @param maxLagSeconds Maximum lag to search in seconds
/// @param minAbsBestCorrelation If best correlation (rectified) is under this value,
/// then signals will be considered to not have valid overlap, and result will be NaN
/// @param searchMode Controls how the best lag is selected
///
/// @return MetricQuery containing lag values corresponding to best cross-correlation
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
MetricQuery<double> lagAtMaxCrossCorrelation (
    const AudioBuffer<SampleType>& bufferA,
    const AudioBuffer<SampleType>& bufferB,
    double maxLagSeconds,
    double minAbsBestCorrelation = 0.5,
    CorrelationSearchMode searchMode = bestAbsoluteCorrelation
)
{
    if (maxLagSeconds < 0.0)
        HART_THROW_OR_RETURN (hart::ValueError, "Maximum lag must be non-negative", {});

    if ((bufferA.hasSampleRate() || bufferB.hasSampleRate()) && bufferA.getSampleRateHz() != bufferB.getSampleRateHz())
        HART_THROW_OR_RETURN (hart::SampleRateError, "Audio buffers must have equal sample rates", {});

    if (minAbsBestCorrelation < 0 || minAbsBestCorrelation > 1.0)
        HART_THROW_OR_RETURN (hart::ValueError, "minAbsBestCorrelation should be in 0..1 range", {});

    typename MetricQuery<double>::ChannelPairMetricEvaluator evaluator =
        [&bufferA, &bufferB, maxLagSeconds, minAbsBestCorrelation, searchMode]
        (size_t channelA, size_t channelB, Slice slice, Unit requestedUnit)
        -> double
    {
        // Should be checked by MetricQuery
        hassert (channelA < bufferA.getNumChannels());
        hassert (channelB < bufferB.getNumChannels());

        if (requestedUnit != Unit::native &&
            requestedUnit != Unit::frames &&
            requestedUnit != Unit::seconds
            )
        {
            HART_THROW_OR_RETURN (hart::UnitError, "Unsupported unit", hart::nan<double>());
        }

        if (requestedUnit == Unit::seconds)
        {
            if (! bufferA.hasSampleRate()
                || ! bufferB.hasSampleRate()
                )
            {
                HART_THROW_OR_RETURN (hart::SampleRateError, "Audio buffers must have sample rate metadata to convert lag to seconds", hart::nan<double>());
            }

            if (hart::floatsEqual (bufferA.getSampleRateHz(), 0.0)
                || hart::floatsEqual (bufferB.getSampleRateHz(), 0.0)
                )
            {
                HART_THROW_OR_RETURN (hart::SampleRateError, "Audio buffers must have non-zero sample rates to convert lag to seconds", hart::nan<double>());
            }
        }

        // This might be a bit too strict. So, if a legit case with two buffers with
        // mismatched lengths presents itself, remove this check and handle it properly.
        if (bufferA.getNumFrames() != bufferB.getNumFrames())
            HART_THROW_OR_RETURN (hart::SizeError, "Audio buffers must have matching n umber of frames", hart::nan<double>());

        if (slice.isEmpty())
            return hart::nan<double>();

        const auto sliceFrameIndices = bufferA.getFrameIndices (slice);
        const size_t sliceStart = sliceFrameIndices.first;
        const size_t sliceStop = sliceFrameIndices.second;
        hassert (sliceStop > sliceStart);
        hassert (sliceStop <= bufferA.getNumFrames());
        hassert (sliceStop <= bufferB.getNumFrames());

        const size_t numFrames = sliceStop - sliceStart;
        hassert (numFrames != 0);

        const double sampleRateHz = bufferA.getSampleRateHz();
        const size_t maxLagFrames = static_cast<size_t> (std::round (maxLagSeconds * sampleRateHz));

        const SampleType* x = bufferA[channelA] + sliceStart;
        const SampleType* y = bufferB[channelB] + sliceStart;

        double bestCorrelation =
            (searchMode == bestSignedCorrelation)
                ? -hart::inf
                : 0.0;

        int bestLagFrames = 0;
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

            bool isBetter = 
                (searchMode == bestSignedCorrelation)
                    ? correlation > bestCorrelation
                    : std::abs (correlation) > std::abs (bestCorrelation);

            if (isBetter)
            {
                bestCorrelation = correlation;
                bestLagFrames = lag;
            }

            if (floatsEqual (std::abs (bestCorrelation), 1.0))
                break;
        }

        if (std::abs (bestCorrelation) < minAbsBestCorrelation)
            hadValidOverlap = false;

        if (! hadValidOverlap)
            return hart::nan<double>();

        switch (requestedUnit)
        {
            case Unit::native:
            case Unit::frames:
                return static_cast<double> (bestLagFrames);

            case Unit::seconds:
                return bestLagFrames / sampleRateHz;

            default:  // Should be unreachable
                HART_THROW_OR_RETURN (hart::UnitError, "Unsupported unit", hart::nan<double>());
        }
    };

    const size_t numPairs = std::min (bufferA.getNumChannels(), bufferB.getNumChannels());
    return MetricQuery<double> (
        std::move (evaluator),
        bufferA.getNumChannels(),
        bufferB.getNumChannels(),
        ChannelSubsets::diagonalChannelPairs (numPairs)
    );
}

}  // namespace hart
