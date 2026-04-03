#pragma once

#include "hart_accurate_sum.hpp"
#include "hart_audio_buffer.hpp"
#include "hart_exceptions.hpp"
#include "hart_utils.hpp"  // nan()

namespace hart
{

/// @ingroup Metrics
template <typename SampleType>
double channelCorrelation (const AudioBuffer<SampleType>& buffer, size_t channelA = 0, size_t channelB = 1)
{
    const double nan = hart::nan<double>();

    if (channelA >= buffer.getNumChannels())
        HART_THROW_OR_RETURN (hart::IndexError, "Channel A index is out of bounds", nan);

    if (channelB >= buffer.getNumChannels())
        HART_THROW_OR_RETURN (hart::IndexError, "Channel B index is out of bounds", nan);

    if (buffer.getNumFrames() == 0)
        return nan;

    // If channel A and B point to the same channel, we still want to go through the whole thing,
    // as it can be either 1.0 or NaN depending on the contents

    const size_t numFrames = buffer.getNumFrames();
    const SampleType* x = buffer[channelA];
    const SampleType* y = buffer[channelB];

    AccurateSum<SampleType> dotProduct = 0.0;
    AccurateSum<SampleType> sumSqX = 0.0;
    AccurateSum<SampleType> sumSqY = 0.0;

    for (size_t frame = 0; frame < numFrames; ++frame)
    {
        const SampleType xVal = x[frame];
        const SampleType yVal = y[frame];

        dotProduct += xVal * yVal;
        sumSqX += xVal * xVal;
        sumSqY += yVal * yVal;
    }

    if (floatsEqual<SampleType> (sumSqX, 0) || floatsEqual<SampleType> (sumSqY, 0))
    {
        return nan;
    }

    return static_cast<double> (dotProduct) / std::sqrt (static_cast<double> (sumSqX * sumSqY));
}

}  // namespace hart
