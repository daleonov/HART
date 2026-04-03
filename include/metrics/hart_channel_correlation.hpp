#pragma once

#include "hart_accurate_sum.hpp"
#include "hart_audio_buffer.hpp"
#include "hart_exceptions.hpp"
#include "hart_utils.hpp"  // nan()

namespace hart
{

/// @brief Calculates zero-lag normalized cross-correlation between two channels of an audio buffer
/// @details
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
/// @param channelA Index of the first channel to compare. Defaults to `0` (left channel).
/// @param channelB Index of the second channel to compare. Defaults to `1` (right channel).
/// @returns Normalized correlation coefficient, or `NaN` if correlation is undefined
/// @tparam SampleType Floating point sample type, typically `float` or `double`
/// @throws hart::IndexError if either channel index is out of bounds
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
