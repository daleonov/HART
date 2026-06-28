#pragma once

#include <algorithm>  // min()

#include "hart_accurate_sum.hpp"
#include "hart_audio_buffer.hpp"
#include "hart_exceptions.hpp"
#include "metrics/hart_metric_query.hpp"
#include "metrics/hart_metrics_common.hpp"  // ChannelSubsets
#include "hart_slice.hpp"
#include "hart_utils.hpp"  // nan(), floatsEqual()
#include "hart_units.hpp"  // Unit

namespace hart
{

/// @brief Calculates error-to-signal ratio (ESR)
/// @details ESR is a useful way to express the degree of similarity between
/// two signals or waveforms, calculated as:
/// 
/// @f[
/// ESR=\frac{\sum_{k=0}^{N-1} (x - y) ^ 2}{\sum_{k=0}^{N-1} x^2}$$
/// @f]
///
/// (sum ((x - y) ** 2) / sum (x ** 2))
///
/// Where x is a signal represented by @p referenceBuffer and y is represented
/// by @p estimatedBuffer. It's a ratio, so appropriate units are `Unit::native`
/// and `Unit::ratio`. ESR = 0 means two signals are identical.
/// @param referenceBuffer A buffer representing x in the formula above
/// @param estimatedBuffer A buffer representing y in the formula above
/// @return  Chainable `MetricQuery`, which calculates per-channel ESR values
/// @ingroup Metrics
template <typename SampleType>
MetricQuery<double> esr (const AudioBuffer<SampleType>& referenceBuffer, const AudioBuffer<SampleType>& estimatedBuffer)
{
    if ((referenceBuffer.hasSampleRate() || estimatedBuffer.hasSampleRate()) && referenceBuffer.getSampleRateHz() != estimatedBuffer.getSampleRateHz())
        HART_THROW_OR_RETURN (hart::SampleRateError, "Audio buffers must have equal sample rates", {});

    if (estimatedBuffer.getNumFrames() != referenceBuffer.getNumFrames())
        HART_THROW_OR_RETURN (hart::SizeError, "Buffers' sizes don't match", hart::nan<double>());

    typename MetricQuery<double>::ChannelPairMetricEvaluator evaluator =
        [&referenceBuffer, &estimatedBuffer]
        (size_t referenceBufferChannel, size_t estimatedBufferChannel, Slice slice, Unit requestedUnit)
        -> double
    {
        hassert (estimatedBuffer.getNumFrames() == referenceBuffer.getNumFrames());

        if (referenceBufferChannel >= referenceBuffer.getNumChannels())
            HART_THROW_OR_RETURN (hart::IndexError, "Reference buffer's channel index is out of bounds", hart::nan<double>());

        if (estimatedBufferChannel >= estimatedBuffer.getNumChannels())
            HART_THROW_OR_RETURN (hart::IndexError, "Estimated buffer's channel index is out of bounds", hart::nan<double>());

        // TODO: Support percent or dB?
        if (requestedUnit != Unit::native && requestedUnit != Unit::ratio)
            HART_THROW_OR_RETURN (hart::UnitError, "ESR does not support requested unit", hart::nan<double>());

        if (slice.isEmpty())
            return hart::nan<double>();

        const auto sliceFrameIndices = referenceBuffer.getFrameIndices (slice);
        const size_t sliceStart = sliceFrameIndices.first;
        const size_t sliceStop = sliceFrameIndices.second;
        hassert (sliceStop > sliceStart);
        hassert (sliceStop <= referenceBuffer.getNumFrames());

        const size_t numFrames = sliceStop - sliceStart;
        hassert (numFrames != 0);

        AccurateSum<double> signalPower;
        AccurateSum<double> noisePower;

        const SampleType* referenceSamples = referenceBuffer[referenceBufferChannel] + sliceStart;
        const SampleType* estimatedSamples = estimatedBuffer[estimatedBufferChannel] + sliceStart;

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
    };

    const size_t numPairs = std::min (referenceBuffer.getNumChannels(), estimatedBuffer.getNumChannels());
    return MetricQuery<double> (
        std::move (evaluator),
        referenceBuffer.getNumChannels(),
        estimatedBuffer.getNumChannels(),
        ChannelSubsets::diagonalChannelPairs (numPairs)
    );
}

}  // namespace hart
