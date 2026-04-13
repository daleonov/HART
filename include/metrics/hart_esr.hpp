#pragma once

#include "hart_accurate_sum.hpp"
#include "hart_audio_buffer.hpp"
#include "hart_exceptions.hpp"
#include "hart_utils.hpp"  // nan(), floatsEqual()

#include "hart_metrics_common.hpp"
//#include "hart_reducers.hpp"

namespace hart
{

template <typename SampleType>
double esr (const AudioBuffer<SampleType>& referenceBuffer, const AudioBuffer<SampleType>& estimatedBuffer, size_t channel = 0)
{
    if (channel >= referenceBuffer.getNumChannels() || channel >= estimatedBuffer.getNumChannels())
        HART_THROW_OR_RETURN (hart::IndexError, "Channel index is out of bounds", hart::nan<double>());

    if (estimatedBuffer.getNumFrames() != referenceBuffer.getNumFrames())
        HART_THROW_OR_RETURN (hart::SizeError, "Buffers' sizes don't match", hart::nan<double>());

    const size_t numFrames = referenceBuffer.getNumFrames();

    if (numFrames == 0)
        return hart::nan<double>();

    AccurateSum<double> signalPower;
    AccurateSum<double> noisePower;

    const SampleType* referenceSamples = referenceBuffer[channel];
    const SampleType* estimatedSamples = estimatedBuffer[channel];

    for (size_t frame = 0; frame < numFrames; ++frame)
    {
        const double x = static_cast<double> (referenceSamples[frame]);
        const double y = static_cast<double> (estimatedSamples[frame]);
        const double noise = x - y;

        signalPower += x * x;
        noisePower += noise * noise;
    }

    if (floatsEqual<double> (signalPower, 0.0))
        return hart::nan<double>();

    return noisePower.getValue() / signalPower.getValue();
}

template <typename SampleType, typename ReducerType>
auto esr (ReducerType reducer, const AudioBuffer<SampleType>& referenceBuffer, const AudioBuffer<SampleType>& estimatedBuffer, std::initializer_list<size_t> channels = {})
    -> ReducerResultType<ReducerType, std::vector<double>::const_iterator>
{
    const auto channelIndicesToProcess = getChannelIndicesToProcess (referenceBuffer, estimatedBuffer, channels);
    std::vector<double> perChannelValues;
    perChannelValues.reserve (channelIndicesToProcess.size());

    for (size_t channel : channelIndicesToProcess)
        perChannelValues.push_back (esr (referenceBuffer, estimatedBuffer, channel));

    return reducer (perChannelValues.begin(), perChannelValues.end());
}

}  // namespace hart
