#pragma once

#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <vector>

#include "hart_accurate_sum.hpp"
#include "hart_audio_buffer.hpp"
#include "hart_exceptions.hpp"
#include "hart_metrics_common.hpp"
#include "hart_reducers.hpp"
#include "hart_utils.hpp"  // nan(), inf, ratioToDecibels()

namespace hart
{

/// @brief Calculates linear crest factor for a single channel of an audio buffer
/// @details
/// Crest factor is defined as the ratio between the absolute peak value and RMS value:
/// @f[
/// \frac{\max_n \left|x[n]\right|}{\sqrt{\frac{1}{N}\sum_n x[n]^2}}
/// @f]
///
/// (`max (abs (x[n])) / sqrt ((1 / N) * sum (x[n]^2))`)
///
/// Calculates values independently for each channel. Use a reducer to
/// get a scalar value (see @ref Reducers). Supports `Unit::linear` (default) and
/// `Unit::dB` units. Decibel conversion is performed as `20 * log10 (x)`, i.e.
/// the amplitude-ratio form, see `hart::ratioToDecibels()`.
///
/// Usage example:
/// @code
/// // One channel, default unit (linear)
/// const double cfLinear = crestFactor (monoBuffer).get();
/// 
/// // One channel, decibels
/// const double cfDb = crestFactor (monoBuffer).as (dB).get();
/// 
/// // Calculates crest factor for each channel as linear value, which is a default (native) unit
/// // for this metric, then returns index of the largest value. Since channel subset is default
/// // (no ch() call), the returns index is the same as channel's index.
/// const size_t mostDynamicChannel = crestFactor (multiChannelBuffer).as (linear).get (argmax());
/// 
/// // Calculates crest factor for channels 0, 3 and 5 in dB, then returns maximum of those three
/// const double cfMaxDb = crestFactor (multiChannelBuffer).as (dB).ch ({0, 3, 5}).get (max());
/// @endcode
///
/// @param buffer Input audio buffer
/// @return  Chainable `MetricQuery`, which calculates crest factor in linear ratio units or dB
///   - Returns `NaN` if the audio buffer contains zero frames.
///   - Returns `inf` if the selected channel is silent, making RMS equal to (or close to) zero.
/// @tparam SampleType Floating point sample type of the audio buffer, typically `float` or `double`
/// @throws hart::IndexError if the channel index is out of bounds
/// @ingroup Metrics
template <typename SampleType>
MetricQuery<double> crestFactor (const AudioBuffer<SampleType>& buffer)
{
    typename MetricQuery<double>::SingleChannelMetricEvaluator evaluator =
        [&buffer]
        (size_t channel, size_t sliceStart, size_t sliceStop, Unit requestedUnit)
        -> double
    {
        // Should be handled by MetricQuery
        hassert (sliceStart < sliceStop);
        hassert (channel < buffer.getNumChannels());

        const size_t numFrames = sliceStop - sliceStart;

        if (numFrames == 0)
            return hart::nan<double>();

        const SampleType* channelData = buffer[channel];

        double peakLinear = 0.0;
        AccurateSum<double> sumSquares;

        for (size_t frame = sliceStart; frame < sliceStop; ++frame)
        {
            const double x = static_cast<double> (channelData[frame]);
            const double absX = std::abs (x);

            if (absX > peakLinear)
                peakLinear = absX;

            sumSquares += x * x;
        }

        const double meanSquare = sumSquares.get<double>() / numFrames;

        if (floatsEqual (meanSquare, 0.0))
            return hart::inf;

        const double rms = std::sqrt (meanSquare);
        const double crestFactorLinear = peakLinear / rms;

        switch (requestedUnit)
        {
            case Unit::native:
            case Unit::linear: return crestFactorLinear;

            case Unit::dB: return hart::ratioToDecibels (crestFactorLinear);

            default: HART_THROW_OR_RETURN (hart::UnitError, "Unsupported unit",  hart::nan<double>());
        }
    };

    std::vector<size_t> defaultChannelsToProcess (buffer.getNumChannels());

    for (size_t i = 0; i < defaultChannelsToProcess.size(); ++i)
        defaultChannelsToProcess[i] = i;

    return MetricQuery<double> (
        std::move (evaluator),
        buffer.getNumChannels(),
        buffer.getNumFrames(),
        std::move (defaultChannelsToProcess)
    );
}

}  // namespace hart
