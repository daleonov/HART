#pragma once

#include "hart_accurate_sum.hpp"
#include "hart_audio_buffer.hpp"
#include "hart_exceptions.hpp"
#include "metrics/hart_metric_query.hpp"
#include "metrics/hart_metrics_common.hpp"  // ChannelSubsets
#include "hart_slice.hpp"
#include "hart_units.hpp"  // Unit
#include "hart_utils.hpp"  // nan()

namespace hart
{

/// @brief Calculates zero-lag normalized cross-correlation between two channels of an audio buffer
/// @details Operates per specified pairs of channels, use a reducer to get a scalar value
/// (see @ref Reducers). If custom channel subset is not specified via `ch()`, defaults to a set of
/// all unique unordered channel pairs, e.g. `{{0, 1}}` for stereo buffer, or
/// `{{0, 1}, {0, 2}, {1, 2}, {1, 3}, {2, 3}}` for 3-channel buffer. A specific order of pairs is
/// not guaranteed, unless you explicitly pass a custom list of channel pairs via `ch()`.
///
/// Returns a unitless value, suppoert `Unit::none` and `Unit::native`, which are the same here,
/// so there's no need to request any unit with a chained `as()` call.
///
/// Usage examples
/// @code
/// // All defaults - correlation between channels 0 and 1 (left and right)
/// channelCorrelation (stereoBuffer).get();
///
/// // Highest value of three cross-correlations (channels 0 vs 1, 0 vs 2 and 1 vs 3) 
/// channelCorrelation (multichannelBuffer).ch ({{0, 1}, {0, 2}, {1, 3}}).get (max());
/// @endcode
///
/// Be careful when you want to specify only one channel pair:
/// @code
/// // /!\ Resolves to 2 matched channel pairs - 0 vs 0 and 1 vs 1.
/// // Probably not what you're looking for!
/// channelCorrelation (stereoBuffer).ch ({0, 1});
/// 
/// // Just one pair - 0 vs 1. Note the double curly braces.
/// channelCorrelation (stereoBuffer).ch ({{0, 1}});
/// @endcode
///
/// Uses the normalized cross-correlation formula:
/// @f[
/// \rho = \frac{\sum_n x[n]\,y[n]}
///              {\sqrt{\left(\sum_n x[n]^2\right)\left(\sum_n y[n]^2\right)}}
/// @f]
///
/// (`sum (x[n] * y[n]) / sqrt (sum (x[n]^2) * sum (y[n]^2))`)
///
/// where `x` and `y` are the selected channels of the same buffer.
///
/// The returned value is in the range `[-1, 1]`:
/// - `1.0` means perfectly correlated channels
/// - `0.0` means no linear correlation
/// - `-1.0` means perfectly inverted polarity
///
/// The function returns `NaN` if correlation is undefined, such as when:
/// - one of the selected channels is silent
/// - the buffer contains zero frames
///
/// @param buffer Input audio buffer
/// @returns Chainable `MetricQuery`, which calculates normalized correlation coefficient
/// per pair of channels, or `NaN` if correlation is undefined
/// @tparam SampleType Floating point sample type, typically `float` or `double`
/// @throws hart::IndexError if either channel index is out of bounds
/// @ingroup Metrics
template <typename SampleType>
MetricQuery<double>  channelCorrelation (const AudioBuffer<SampleType>& buffer)
{
    typename MetricQuery<double>::ChannelPairMetricEvaluator evaluator =
        [&buffer]
        (size_t channelA, size_t channelB, Slice slice, Unit requestedUnit)
        -> double
    {
        const double nan = hart::nan<double>();

        if (channelA >= buffer.getNumChannels())
            HART_THROW_OR_RETURN (hart::IndexError, "Channel A index is out of bounds", nan);

        if (channelB >= buffer.getNumChannels())
            HART_THROW_OR_RETURN (hart::IndexError, "Channel B index is out of bounds", nan);

        if (requestedUnit != Unit::native && requestedUnit != Unit::none)
            HART_THROW_OR_RETURN (hart::UnitError, "Channel correlation cannot be calculated in a requested unit", nan);

        if (slice.isEmpty())
            return hart::nan<double>();

        const auto sliceFrameIndices = buffer.getFrameIndices (slice);
        const size_t sliceStart = sliceFrameIndices.first;
        const size_t sliceStop = sliceFrameIndices.second;
        hassert (sliceStart < sliceStop);
        hassert (sliceStop - sliceStart != 0);
        hassert (sliceStop <= buffer.getNumFrames());

        // If channel A and B point to the same channel, we still want to go through the whole thing,
        // as it can be either 1.0 or NaN depending on the contents

        const SampleType* channelAData = buffer[channelA];
        const SampleType* channelBData = buffer[channelB];

        AccurateSum<double> dotProduct { 0.0 };
        AccurateSum<double> sumSqChannelA { 0.0 };
        AccurateSum<double> sumSqChannelB { 0.0 };

        for (size_t frame = sliceStart; frame < sliceStop; ++frame)
        {
            const double channelAValue = static_cast<double> (channelAData[frame]);
            const double channelBValue = static_cast<double> (channelBData[frame]);

            dotProduct += channelAValue * channelBValue;
            sumSqChannelA += channelAValue * channelAValue;
            sumSqChannelB += channelBValue * channelBValue;
        }

        if (floatsEqual<double> (sumSqChannelA, 0.0) || floatsEqual<double> (sumSqChannelB, 0.0))
            return nan;

        return dotProduct / std::sqrt (sumSqChannelA * sumSqChannelB);
    };

    const size_t numChannels = buffer.getNumChannels();
    return MetricQuery<double> (
        std::move (evaluator),
        numChannels,
        numChannels,
        ChannelSubsets::upperTriangleChannelPairs (numChannels)
    );
}

}  // namespace hart
