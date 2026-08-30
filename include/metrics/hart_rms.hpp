#pragma once

#include <cmath>  // sqrt()
#include <complex>

#include "hart_accurate_sum.hpp"
#include "hart_audio_buffer.hpp"
#include "hart_exceptions.hpp"
#include "metrics/hart_metric_query.hpp"
#include "metrics/hart_metrics_common.hpp"  // ChannelSubsets
#include "hart_slice.hpp"
#include "hart_units.hpp"  // Unit
#include "hart_utils.hpp"  // nan(), ratioToDecibels()

// TODO: Document RMS for Spectrum
// TODO: Implement RMS for IR

namespace hart
{

/// @brief  Calculates root mean square (RMS) of an audio buffer
/// @details  RMS a metric that expresses the average magnitude, or effective
/// energy level, of an audio signal over time. It is commonly used to estimate
/// perceived loudness, and to measure overall signal level.
/// 
/// RMS is calculated this way:
/// @f[
/// \mathrm{RMS} = \sqrt{\frac{1}{N} \sum_{n=0}^{N-1} x[n]^2}
/// @f]
/// (RMS = sqrt((1 / N) * sum(x[n] ** 2))),
///
/// where x[n] is audio sample value from one channel, and N
/// is number of frames in the provided buffer.
///
/// Can be expressed as ratio or decibels, supports `Unit:::native`,
/// `Unit::linear`, `Unit::dB` units. Value in decibels is
/// calculated as a ratio, not power.
///
/// @tparam SampleType 
/// @param buffer Audio buffer to calculate RMS at
/// @return Chainable `MetricQuery` object, which calculates RMS as linear ratio
/// or decibels
/// @ingroup Metrics
template <typename SampleType>
MetricQuery<double> rms (const AudioBuffer<SampleType>& buffer)
{
    typename MetricQuery<double>::SingleChannelMetricEvaluator evaluator =
        [&buffer]
        (size_t channel, Slice slice, Unit requestedUnit)
        -> double
    {
        hassert (channel < buffer.getNumChannels());

        if (slice.isEmpty())
            return hart::nan<double>();

        const auto sliceFrameIndices = buffer.getFrameIndices (slice);
        const size_t sliceStart = sliceFrameIndices.first;
        const size_t sliceStop = sliceFrameIndices.second;
        const size_t numFrames = sliceStop - sliceStart;
        hassert (numFrames != 0);
        hassert (sliceStart < sliceStop);
        hassert (sliceStop <= buffer.getNumFrames());

        const SampleType* channelData = buffer[channel];
        AccurateSum<SampleType> sumSquares;

        for (size_t frame = sliceStart; frame < sliceStop; ++frame)
        {
            const SampleType x = channelData[frame];
            sumSquares += x * x;
        }

        const double rmsLinear = std::sqrt (sumSquares.template get<double>() / numFrames);

        switch (requestedUnit)
        {
            case Unit::native:
            case Unit::linear: return rmsLinear;

            case Unit::dB: return hart::ratioToDecibels (rmsLinear);

            default: HART_THROW_OR_RETURN (hart::UnitError, "Unsupported unit",  hart::nan<double>());
        }
    };

    const size_t numChannels = buffer.getNumChannels();
    return MetricQuery<double> (
        std::move (evaluator),
        numChannels,
        ChannelSubsets::allChannels (numChannels)
    );
}

/// @brief  Calculates root mean square (RMS) of a spectrum
/// @ingroup Metrics
inline MetricQuery<double> rms (const Spectrum& spectrum)
{
    MetricQuery<double>::SingleChannelMetricEvaluator evaluator =
        [&spectrum]
        (size_t channel, Slice slice, Unit requestedUnit)
        -> double
    {
        if (spectrum.getFFTSize() == 0)
            return hart::nan<double>();

        hassert (channel < spectrum.getNumChannels());
        hassert (! std::isnan (spectrum.getSampleRateHz()));

        const std::pair<size_t, size_t> binIndices = spectrum.getBinIndices (slice);
        size_t startBin = binIndices.first;
        size_t stopBin = binIndices.second;

        hassert (startBin < stopBin);
        hassert (stopBin <= spectrum.getNumBins());

        if (slice.isEmpty())
            return hart::nan<double>();

        const std::complex<double>* channelData = spectrum[channel];
        AccurateSum<double> sumSquaredMagnitudes;

        constexpr size_t dcBin = 0;

        if (startBin == dcBin)
        {
            // DC bin is not doubled
            sumSquaredMagnitudes += std::norm (channelData[0]);
            ++startBin;
        }

        const bool includesNyquist = ((spectrum.getFFTSize() & 0x01) == 0) && stopBin == spectrum.getNumBins();
        hassert (stopBin != 0);

        if (includesNyquist)
        {
            // Nyquist bin is not doubled
            sumSquaredMagnitudes += std::norm (channelData[stopBin - 1]);
            --stopBin;
        }

        for (size_t bin = startBin; bin < stopBin; ++bin)
        {
            // Those bins are doubled, since the hart::Spectrum is one-sided
            sumSquaredMagnitudes += 2.0 * std::norm (channelData[bin]);
        }

        // TODO: Probably divide by sqrt (audioSize * FFTsize) here, instead of FFTSize, assuming zero-padded FFT?
        const double rmsLinear = std::sqrt (sumSquaredMagnitudes.getValue()) / static_cast<double> (spectrum.getFFTSize());

        switch (requestedUnit)
        {
            case Unit::native:
            case Unit::linear: return rmsLinear;

            case Unit::dB: return hart::ratioToDecibels (rmsLinear);

            default: HART_THROW_OR_RETURN (hart::UnitError, "Unsupported unit",  hart::nan<double>());
        }
    };

    const size_t numChannels = spectrum.getNumChannels();
    return MetricQuery<double> (
        std::move (evaluator),
        numChannels,
        ChannelSubsets::allChannels (numChannels)
    );
}

}  // namespace hart
