#pragma once

#include <cmath>  // min()

#include "hart_accurate_sum.hpp"
#include "hart_audio_buffer.hpp"
#include "hart_exceptions.hpp"
#include "metrics/hart_metric_query.hpp"
#include "metrics/hart_metrics_common.hpp"  // ChannelSubsets
#include "hart_slice.hpp"
#include "hart_utils.hpp"  // nan(), floatsEqual(), floatsNotEqual(), powerToDecibels()
#include "hart_units.hpp"  // Unit

namespace hart
{
// TODO: Document it

/// @brief Calculates signal to noise ratio (SNR)
/// @ingroup Metrics
template <typename SampleType>
MetricQuery<double> snr (const AudioBuffer<SampleType>& signalPlusNoise, const AudioBuffer<SampleType>& signal)
{
    if (! signalPlusNoise.hasSampleRate() || signalPlusNoise.getSampleRateHz() < 0.0 || floatsEqual (signalPlusNoise.getSampleRateHz(), 0.0))
        HART_THROW_OR_RETURN (SampleRateError, "signalPlusNoise must have a valid sample rate", nan<double>());

    if (! signal.hasSampleRate() || signal.getSampleRateHz() < 0.0 || floatsEqual (signal.getSampleRateHz(), 0.0))
        HART_THROW_OR_RETURN (SampleRateError, "signal must have a valid sample rate", nan<double>());

    if (floatsNotEqual (signalPlusNoise.getSampleRateHz(), signal.getSampleRateHz()))
        HART_THROW_OR_RETURN (SampleRateError, "Both provided buffers should have same saple rate", nan<double>());

    MetricQuery<double>::SingleChannelMetricEvaluator evaluator =
        [&signalPlusNoise, &signal]
        (size_t channel, Slice slice, Unit requestedUnit)
        -> double
    {
        const double sampleRateHz = signal.getSampleRateHz();

        if (channel >= signalPlusNoise.getNumChannels())
            HART_THROW_OR_RETURN (hart::IndexError, "Channel index is out of bounds for the signalPlusNoise buffer", nan<double>());

        if (channel >= signal.getNumChannels())
            HART_THROW_OR_RETURN (hart::IndexError, "Channel index is out of bounds for the signal buffer", nan<double>());

        if (slice.isEmpty())
            return nan<double>();

        const auto sliceFrameIndices = signal.getFrameIndices (slice);
        const size_t sliceStart = sliceFrameIndices.first;
        const size_t sliceStop = sliceFrameIndices.second;
        hassert (sliceStop > sliceStart);
        hassert (sliceStop <= signal.getNumFrames());

        const size_t numFrames = sliceStop - sliceStart;
        hassert (numFrames != 0);

        AccurateSum<double> signalEnergy;
        AccurateSum<double> noiseEnergy;

        const SampleType* signalChannelData = signal[channel] + sliceStart;
        const SampleType* signalPlusNoiseChannelData = signalPlusNoise[channel] + sliceStart;

        for (size_t frame = 0; frame < numFrames; ++frame)
        {
            const double s = static_cast<double> (signalChannelData[frame]);
            const double spn = static_cast<double> (signalPlusNoiseChannelData[frame]);
            const double n = spn - s;

            signalEnergy += s * s;
            noiseEnergy += n * n;
        }

        if (floatsEqual<double> (signalEnergy, 0.0))
            return nan<double>();

        // Congrats - no noise at all!
        if (floatsEqual<double> (noiseEnergy, 0.0))
            return inf;  // Both ratio and dB are inf here 

        const double snrRatio = signalEnergy.getValue() / noiseEnergy.getValue();

        switch (requestedUnit)
        {
            case Unit::native:
            case Unit::linear: return snrRatio;

            case Unit::dB: return hart::powerToDecibels (snrRatio);

            default: HART_THROW_OR_RETURN (hart::UnitError, "Unsupported unit",  nan<double>());
        }
    };

    const size_t numChannels = std::min (signal.getNumChannels(), signalPlusNoise.getNumChannels());
    return MetricQuery<double> (
        std::move (evaluator),
        numChannels,
        ChannelSubsets::allChannels (numChannels)
    );
}

}  // namespace hart
