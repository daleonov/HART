#pragma once

#include <memory>  // make_shared()

#include "hart_audio_buffer.hpp"
#include "hart_exceptions.hpp"
#include "metrics/hart_metric_query.hpp"
#include "metrics/hart_metrics_common.hpp"  // ChannelSubsets
#include "hart_slice.hpp"
#include "metrics/hart_snr.hpp"
#include "hart_utils.hpp"  // nan(), floatsEqual(), floatsNotEqual()
#include "hart_units.hpp"  // Unit

namespace hart
{

/// @brief Calculates signal-to-aliasing-noise ratio (a.k.a. SNRA, SNRa or sometimes SANR)
/// @details SNRa estimates the amount of aliasing introduced by an audio
/// processing algorithm by comparing its output at the native sample rate
/// against a higher-sample-rate render.
///
/// SNRa is particularly suitable for non-linear DSP such as waveshapers,
/// saturation, clipping, nonlinear filters, limiters, nonlinear circuit
/// models, and anti-aliased waveform generators. It may be unsuitable for
/// algorithms whose behavior changes materially with sample rate, such as
/// fixed-rate processing engines or some fixed-size STFT-based algorithms.
///
/// This metric implies a specific experiment:
/// 1. You render audio through your DSP at native sample rate. This will be
/// considered as "signal + aliasing noise", and amount of aliasing will be
/// evaluated in this piece of audio.
/// 2. Render audio through your DSP at the same starting state, and same
/// duration of audio (expressed in seconds, not frames), but at higher sample
/// rate. Due to high sample rate, this will be consiudered as "ideal signal
/// with no aliasing", assuming selected sample rate is high enough to make
/// aliasing negligible.
/// 3. Pass the `AudioBuffer`'s to this metric. They're expected to have
/// matching lengths in seconds, but different sample rates - all resampling
/// will be done by this metric internally.
///
/// You may pick the exact high sample rate value yourself.
/// This sample rate doesn't have to be an integer multiple o the "native"
/// sample rate, although typically you might still use something like 4x or
/// 8x of native sample rate. Obviously, the higher the better.
///
/// Keep in mind that "high sample rate" refers to a "host" sample rate, and
/// doesn't take into account any internal oversampling your DSP testee might
/// have. If your DSP has internal oversampling, keep it on in both renders at
/// the same setting. So, if your DSP has internal 4x OS, and native SR is
/// 44.1 kHz, you keep internal oversampling on, and do a first render at
/// 44.1 kHz, and then a second render at, say, 196 kHz (which, assuming your
/// DSP does 4x OS internally, will result in a whopping 784 kHz internally,
/// but HART renders everuthing offline, so you don't have to worry about
/// potential non-realtime performance here).
///
/// The reference (high SR) buffer is internally resampled to the native sample
/// rate. Aliasing noise is then defined as the sample-by-sample difference
/// betweenthe native-rate output and this resampled reference.
///
/// SNRa is calculated as:
/// @f[
/// \mathrm{SNR_a}
/// =
/// \frac
/// {\sum_{n=0}^{N-1} r[n]^2}
/// {\sum_{n=0}^{N-1} \left(x[n] - r[n]\right)^2}
/// @f]
///
/// (SNRa = sum(r[n] ** 2) / sum((x[n] - r[n]) ** 2)),
///
/// where `x[n]` is a sample from the output rendered at the native sample rate,
/// `r[n]` is the corresponding sample from the higher-sample-rate reference
/// after it has been resampled to the native sample rate, and `N` is the number
/// of frames being analyzed.
///
/// Higher values indicate less aliasing, so higher is better. A perfectly
/// matching native-rate and reference signal produces positive infinity, and
/// this metric will return `+inf`.
///
/// Can be requested as an energy ratio or decibels. Supports `Unit::ratio`,
/// `Unit::native` (default, same as `Unit::ratio`), and `Unit::dB`.
/// Values in decibels are calculated as a power ratio:
///
/// @f[
/// \mathrm{SNR_{a}},dB = 10 \log_{10}\left(\mathrm{SNR_a}\right)
/// @f]
///
/// (SNRa_dB = 10 * log10(SNRa)).
///
/// As mentioned above, this metric assumes that the two buffers are equivalent
/// renders of the same algorithm and state, with the reference buffer rendered
/// at a higher sample rate. In particular:
///
/// - Changing the sample rate must preserve the intended behavior of the
///   algorithm: i.e. all the timings, frequencies etc. must retain their
///   real-world values in their physical units, taking sample rate into account.
///   Same goes fot FFT sizes.
/// - The higher sample rate must actually propagate into the processing being
///   evaluated rather than being internally converted back to a fixed rate.
///   E.g. ML-based DSP models runing at fixed SR internally do not qualify.
/// - Both renders must begin from equivalent state and receive equivalent
///   control and modulation trajectories.
/// - Stochastic processing must be deterministic between the two renders, or
///   otherwise reproduce an equivalent stochastic trajectory.
/// - If your DSP algorithm involves any internal oversampling, make sure to
///   keep it on in both renders, and at the same setting.
///
/// The reference audio resampling operation may introduce boundary transients
/// at the beginning and end of the buffer. For accurate measurements, render
/// guard regions around the desired analysis interval, and use `MetricQuery::at()`
/// with an appropriate `hart::Slice` to measure only an interior, settled slice
/// of audio.
///
/// See `tests/test_metrics.cpp`, in particular the
/// "Metrics - SNRa - Distortion effect with aliasing" and
/// "Metrics - SNRa - Clean effect with no aliasing" tests, for complete
/// experiment examples.
///
/// @tparam SampleType Type of audio buffers' values, typically `float` or `double`
/// @param estimatedBufferAtNativeSR Output rendered at the native sample rate.
/// This will be considered to be a "signal + aliasing noise" when estimating SNRa.
/// @param referenceBufferAtHighSR Equivalent reference output rendered at a
/// higher sample rate. This audio buffer will be considered to be an ideal
/// signal with no aliasing ("signal") when estimating SNRa. This signal should
/// be captured at a higher sample rate that `estimatedBufferAtNativeSR`, and will
/// be internally resampled. Also, note that "high sample rate" refers to a "host"
/// sample rate, and doesn't take into account any internal oversampling your DSP
/// testee might have.
/// @return Chainable `MetricQuery` object which calculates SNRa as a linear
/// energy ratio or in decibels. May return `NaN` or `+inf`.
/// @ingroup Metrics
template <typename SampleType>
MetricQuery<double> snra (const AudioBuffer<SampleType>& estimatedBufferAtNativeSR, const AudioBuffer<SampleType>& referenceBufferAtHighSR)
{
    if (! estimatedBufferAtNativeSR.hasSampleRate() || estimatedBufferAtNativeSR.getSampleRateHz() < 0.0 || floatsEqual (estimatedBufferAtNativeSR.getSampleRateHz(), 0.0))
        HART_THROW_OR_RETURN (SampleRateError, "bufferAtNativeSR must have a valid sample rate", nan<double>());

    if (! referenceBufferAtHighSR.hasSampleRate() || referenceBufferAtHighSR.getSampleRateHz() < 0.0 || floatsEqual (referenceBufferAtHighSR.getSampleRateHz(), 0.0))
        HART_THROW_OR_RETURN (SampleRateError, "referenceBufferAtHighSR must have a valid sample rate", nan<double>());

    if (referenceBufferAtHighSR.getSampleRateHz() < estimatedBufferAtNativeSR.getSampleRateHz() || floatsEqual (referenceBufferAtHighSR.getSampleRateHz(), estimatedBufferAtNativeSR.getSampleRateHz()))
        HART_THROW_OR_RETURN (SampleRateError, "bufferAtHighSR must have sample rate higher than bufferAtNativeSR for proper SANR estimation", nan<double>());

    if (floatsNotEqual (estimatedBufferAtNativeSR.getLengthSeconds(), referenceBufferAtHighSR.getLengthSeconds()))
        HART_THROW_OR_RETURN (SizeError, "Both audio buffers must have same length in seconds", nan<double>());

    // This one is perhaps a bit too strict, but still justified
    if (estimatedBufferAtNativeSR.getNumChannels() != referenceBufferAtHighSR.getNumChannels())
        HART_THROW_OR_RETURN (ChannelLayoutError, "Both buffers have to be rendered with same number of channels to ensure identical DSP state", nan<double>());

    const double nativeSampleRateHz = estimatedBufferAtNativeSR.getSampleRateHz();
    const std::shared_ptr<AudioBuffer<SampleType>> referenceBufferAtNativeSR =
        std::make_shared<AudioBuffer<SampleType>> (referenceBufferAtHighSR.resample (nativeSampleRateHz));

    // Those two may probably differ by a frame or so, so it's okay to loosen this check once it trips over a legit case
    hassert (estimatedBufferAtNativeSR.getNumFrames() == referenceBufferAtNativeSR->getNumFrames());

    // A practical case with mismatched channel numbers is highly unlikely, so channel number match is ensured above
    hassert (estimatedBufferAtNativeSR.getNumChannels() == referenceBufferAtNativeSR->getNumChannels());

    MetricQuery<double>::SingleChannelMetricEvaluator evaluator =
        [&estimatedBufferAtNativeSR, referenceBufferAtNativeSR]
        (size_t channel, Slice slice, Unit requestedUnit)
        -> double
    {
        hassert (referenceBufferAtNativeSR != nullptr);

        // Those two may probably differ by a frame or so, so it's okay to loosen this check once it trips over a legit case
        hassert (estimatedBufferAtNativeSR.getNumFrames() == referenceBufferAtNativeSR->getNumFrames());

        // A practical case with mismatched channel numbers is highly unlikely, so channel number match is ensured above
        hassert (estimatedBufferAtNativeSR.getNumChannels() == referenceBufferAtNativeSR->getNumChannels());

        if (channel >= estimatedBufferAtNativeSR.getNumChannels())
            HART_THROW_OR_RETURN (hart::IndexError, "Channel index is out of bounds", nan<double>());

        if (slice.isEmpty())
            return nan<double>();

        // We'll allow non-full slices, just to be consistent with the reso of the metrics
        const auto sliceFrameIndices = estimatedBufferAtNativeSR.getFrameIndices (slice);
        const size_t sliceStart = sliceFrameIndices.first;
        const size_t sliceStop = sliceFrameIndices.second;
        hassert (sliceStop > sliceStart);
        hassert (sliceStop <= estimatedBufferAtNativeSR.getNumFrames());

        const size_t numFrames = sliceStop - sliceStart;
        hassert (numFrames != 0);

        AccurateSum<double> signalEnergy;
        AccurateSum<double> aliasingNoiseEnergy;

        const SampleType* referenceChannelData = (*referenceBufferAtNativeSR)[channel] + sliceStart;
        const SampleType* estimatedChannelData = estimatedBufferAtNativeSR[channel] + sliceStart;

        for (size_t frame = 0; frame < numFrames; ++frame)
        {
            const double x = static_cast<double> (referenceChannelData[frame]);
            const double y = static_cast<double> (estimatedChannelData[frame]);
            const double noise = x - y;

            signalEnergy += x * x;
            aliasingNoiseEnergy += noise * noise;
        }

        if (floatsEqual<double> (signalEnergy, 0.0))
            return nan<double>();

        if (floatsEqual<double> (aliasingNoiseEnergy, 0.0))
            return inf;  // Congrats - no noise at all!

        const double sanrRatio = signalEnergy.getValue() / aliasingNoiseEnergy.getValue();

        switch (requestedUnit)
        {
            case Unit::native:
            case Unit::ratio: return sanrRatio;

            case Unit::dB: return hart::powerToDecibels (sanrRatio);

            default: HART_THROW_OR_RETURN (hart::UnitError, "Unsupported unit",  nan<double>());
        }
    };

    hassert (estimatedBufferAtNativeSR.getNumChannels() == referenceBufferAtHighSR.getNumChannels());
    const size_t numChannels = estimatedBufferAtNativeSR.getNumChannels();
    return MetricQuery<double> (
        std::move (evaluator),
        numChannels,
        ChannelSubsets::allChannels (numChannels)
    );
}

}  // namespace hart
