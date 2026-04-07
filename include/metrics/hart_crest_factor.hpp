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

// TODO: Add docs to those functions

namespace hart
{

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

template <typename SampleType, typename ReducerType>
ReducerResultType<ReducerType, std::vector<double>::const_iterator>
crestFactorLinear (ReducerType reducer, const AudioBuffer<SampleType>& buffer, std::initializer_list<size_t> channels = {})
{
    const auto channelIndicesToProcess = getChannelIndicesToProcess (buffer, channels);
    std::vector<double> perChannelValues;
    perChannelValues.reserve (channelIndicesToProcess.size());

    for (size_t channel : channelIndicesToProcess)
        perChannelValues.push_back (crestFactorLinear (buffer, channel));

    return reducer (perChannelValues.begin(), perChannelValues.end());
}

template <typename SampleType>
double crestFactorDb (const AudioBuffer<SampleType>& buffer, size_t channel = 0)
{
    return ratioToDecibels (crestFactorLinear (buffer, channel));
}

template <typename SampleType, typename ReducerType>
ReducerResultType<ReducerType, std::vector<double>::const_iterator>
crestFactorDb (ReducerType reducer, const AudioBuffer<SampleType>& buffer, std::initializer_list<size_t> channels = {})
{
    const auto channelIndicesToProcess = getChannelIndicesToProcess (buffer, channels);
    std::vector<double> perChannelValues;
    perChannelValues.reserve (channelIndicesToProcess.size());

    for (size_t channel : channelIndicesToProcess)
        perChannelValues.push_back (crestFactorDb (buffer, channel));

    return reducer (perChannelValues.begin(), perChannelValues.end());
}

}  // namespace hart
