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

/// @brief Calculates signal-to-noise ratio (SNR)
/// @details SNR expresses the ratio between the energy of a reference signal
/// and the energy of the error, or noise, present in an estimated signal.
///
/// The noise component is calculated as the sample-by-sample difference
/// between the estimated and reference signals.
///
/// SNR is calculated this way:
/// @f[
/// \mathrm{SNR}
/// =
/// \frac
/// {\sum_{n=0}^{N-1} r[n]^2}
/// {\sum_{n=0}^{N-1} \left(x[n] - r[n]\right)^2}
/// @f]
///
/// (SNR = sum(r[n] ** 2) / sum((x[n] - r[n]) ** 2)),
///
/// where x[n] is a sample from the estimated signal, r[n] is the
/// corresponding sample from the reference signal, and N is the number of
/// frames being analyzed.
///
/// Higher values indicate a closer match to the reference signal, and thus
/// lower noise. Identical signals produce positive infinity, and this metric
/// will return `+inf` in those cases.
///
/// Can be expressed as an energy ratio or decibels. Supports `Unit::ratio`,
/// `Unit::native` (same as ratio), and `Unit::dB`. Values in decibels are
/// calculated as a power ratio:
///
/// @f[
/// \mathrm{SNR_{dB}} = 10 \log_{10}\left(\mathrm{SNR}\right)
/// @f]
///
/// (SNR_dB = 10 * log10(SNR)).
///
/// The two buffers are expected to represent aligned versions of the same
/// signal. Differences in gain, latency, phase, or other deterministic signal
/// properties are included in the measured noise/error.
///
/// @tparam SampleType
/// @param signalPlusNoise Estimated or measured signal
/// @param signal Reference signal to compare against
/// @return Chainable `MetricQuery` object which calculates SNR as a linear
/// energy ratio or in decibels. May return `NaN` or `+inf`.
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
            case Unit::ratio: return snrRatio;

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
