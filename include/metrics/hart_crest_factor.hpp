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
/// This overload measures one channel only. For the multi-channel overload, see
/// `crestFactorLinear (ReducerType reducer, const AudioBuffer&, std::initializer_list<size_t>)`.
/// For the decibel version, see `crestFactorDb()`.
///
/// @param buffer Input audio buffer
/// @param channel Zero-based channel index to measure. You may use values from @ref hart::Channel
/// or @ref hart::MidSideChannel wherever appropriate.
/// @return Crest factor in linear ratio units
///   - Returns `NaN` if the audio buffer contains zero frames.
///   - Returns `inf` if the selected channel is silent, making RMS equal to (or close to) zero.
/// @tparam SampleType Floating point sample type of the audio buffer, typically `float` or `double`
/// @throws hart::IndexError if the channel index is out of bounds
/// @ingroup Metrics
template <typename SampleType>
double crestFactorLinear (const AudioBuffer<SampleType>& buffer, size_t channel = 0)
{
    if (channel >= buffer.getNumChannels())
        HART_THROW_OR_RETURN (hart::IndexError, "Channel index is out of bounds", hart::nan<double>());

    const size_t numFrames = buffer.getNumFrames();

    if (numFrames == 0)
        return hart::nan<double>();

    const SampleType* channelData = buffer[channel];

    double peak = 0.0;
    AccurateSum<double> sumSquares;

    for (size_t frame = 0; frame < numFrames; ++frame)
    {
        const double x = static_cast<double> (channelData[frame]);
        const double absX = std::abs (x);

        if (absX > peak)
            peak = absX;

        sumSquares += x * x;
    }

    const double meanSquare = sumSquares.get<double>() / numFrames;

    if (floatsEqual (meanSquare, 0.0))
        return hart::inf;

    const double rms = std::sqrt (meanSquare);
    return peak / rms;
}

/// @brief Calculates linear crest factor per channel, then processes the results via provided reducer
/// @details
/// This overload calculates linear crest factor independently for each selected channel, then
/// passes the resulting sequence of per-channel values to a reducer from the @ref Reducers group.
///
/// For the single-channel overload, see `crestFactorLinear (const AudioBuffer&, size_t)`.
/// For the decibel version, see `crestFactorDb()`.
///
/// @param reducer Callable reducer that accepts two iterators over per-channel crest factor values
/// @param buffer Input audio buffer
/// @param channels Optional list of zero-based channel indices to measure. You may use values from
/// @ref hart::Channel or @ref hart::MidSideChannel where appropriate.
/// If this list is empty, all channels of the buffer will be processed. The order of values handed
/// to the reducer matches the order of channel indices in `channels`, or natural channel order if
/// `channels` is empty.
/// @returns Whatever is returned by the supplied reducer, see @ref Reducers
/// @tparam SampleType Floating point sample type of the audio buffer, typically `float` or `double`
/// @tparam ReducerType Type of the reducer callable, see @ref Reducers
/// @throws hart::IndexError if any channel index is out of bounds
/// @ingroup Metrics
template <typename SampleType, typename ReducerType>
auto crestFactorLinear (ReducerType reducer, const AudioBuffer<SampleType>& buffer, std::initializer_list<size_t> channels = {})
    -> ReducerResultType<ReducerType, std::vector<double>::const_iterator>
{
    const auto channelIndicesToProcess = getChannelIndicesToProcess (buffer, channels);
    std::vector<double> perChannelValues;
    perChannelValues.reserve (channelIndicesToProcess.size());

    for (size_t channel : channelIndicesToProcess)
        perChannelValues.push_back (crestFactorLinear (buffer, channel));

    return reducer (perChannelValues.begin(), perChannelValues.end());
}

/// @brief Calculates crest factor in decibels for a single channel of an audio buffer
/// @details
/// This is the decibel form of `crestFactorLinear()`. It calculates linear crest factor first,
/// then converts it to decibels.
///
/// Crest factor is defined as the ratio between the absolute peak value and RMS value:
/// @f[
/// \frac{\max_n \left|x[n]\right|}{\sqrt{\frac{1}{N}\sum_n x[n]^2}}
/// @f]
///
/// (`max (abs (x[n])) / sqrt ((1 / N) * sum (x[n]^2))`)
///
/// Decibel conversion is performed as `20 * log10 (x)`, i.e. the amplitude-ratio form,
/// see `hart::ratioToDecibels()`.
/// @param buffer Input audio buffer
/// @param channel Zero-based channel index to measure. You may use values from @ref hart::Channel
/// or @ref hart::MidSideChannel where appropriate.
/// @returns Crest factor in decibels
///   - Returns `NaN` if the audio buffer contains zero frames.
///   - Returns `inf` if the selected channel is silent, making RMS equal to (or close to) zero.
/// @tparam SampleType Floating point sample type of the audio buffer, typically `float` or `double`
/// @throws hart::IndexError if the channel index is out of bounds
/// @ingroup Metrics
template <typename SampleType>
double crestFactorDb (const AudioBuffer<SampleType>& buffer, size_t channel = 0)
{
    return ratioToDecibels (crestFactorLinear (buffer, channel));
}

/// @brief Calculates crest factor in decibels per channel, then reduces the results
/// @details
/// This is the multi-channel decibel form of @ref crestFactorLinear. It calculates crest factor
/// in decibels independently for each selected channel, then passes the resulting sequence of
/// per-channel values to a reducer from the @ref Reducers group.
///
/// If `channels` is empty, all channels of the buffer are processed. The order of values handed to
/// the reducer matches the order of channel indices in `channels`, or natural channel order if
/// `channels` is empty.
///
/// Decibel conversion is performed as `20 * log10 (x)`, i.e. the amplitude-ratio form,
/// see `hart::ratioToDecibels()`.
///
/// For the single-channel overload, see `crestFactorDb (const AudioBuffer&, size_t)`.
/// For the linear version, see `crestFactorLinear()`.
///
/// @param reducer Callable reducer that accepts two iterators over per-channel crest factor values
/// expressed in decibels, see @ref Reducers
/// @param buffer Input audio buffer
/// @param channels Optional list of zero-based channel indices to measure. You may use values from
/// @ref hart::Channel or @ref hart::MidSideChannel where appropriate.
/// @returns Whatever value is returned by the supplied reducer, see @ref Reducers
/// @tparam SampleType Floating point sample type of the audio buffer, typically `float` or `double`
/// @tparam ReducerType Type of the reducer callable
/// @throws hart::IndexError if any channel index is out of bounds
/// @ingroup Metrics
template <typename SampleType, typename ReducerType>
auto crestFactorDb (ReducerType reducer, const AudioBuffer<SampleType>& buffer, std::initializer_list<size_t> channels = {})
    -> ReducerResultType<ReducerType, std::vector<double>::const_iterator>
{
    const auto channelIndicesToProcess = getChannelIndicesToProcess (buffer, channels);
    std::vector<double> perChannelValues;
    perChannelValues.reserve (channelIndicesToProcess.size());

    for (size_t channel : channelIndicesToProcess)
        perChannelValues.push_back (crestFactorDb (buffer, channel));

    return reducer (perChannelValues.begin(), perChannelValues.end());
}

}  // namespace hart
