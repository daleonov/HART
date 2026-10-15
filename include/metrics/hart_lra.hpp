#pragma once

#include <algorithm>  // any_of()
#include <cmath>  // isfinite(), round()
#include <limits>
#include <utility>
#include <vector>

#include "hart_audio_buffer.hpp"
#include "hart_reducers.hpp"  // percentile
#include "metrics/hart_loudness_common.hpp"
#include "metrics/hart_metric_query.hpp"
#include "hart_utils.hpp"  // nan(), Interpolation

namespace hart
{
namespace LRA
{

/// @brief Reduces selected channels to programme Loudness Range in LU.
/// @details Combines 3 s channel energies before the -70 LUFS absolute gate and
/// -20 LU relative gate, then subtracts the 10th percentile from the 95th.
/// Positions follow the selected channel order, with the same inference as LUFS.
/// @ingroup Reducers
struct reduce
{
    explicit reduce (std::vector<SpeakerPosition> speakerPositions = {}) :
        m_layout (std::move (speakerPositions))
    {
    }

    template <typename IteratorType>
    double operator() (IteratorType begin, IteratorType end) const
    {
        const std::vector<double> energies = hart::internal::combineLoudnessEnergies (
            begin,
            end,
            hart::internal::LoudnessWindow::shortTerm,
            m_layout
            );

        if (energies.empty())
            return hart::nan<double>();

        const bool containsInvalidEnergy = std::any_of (
            energies.begin(),
            energies.end(),
            [] (double energy) { return ! std::isfinite (energy); }
            );

        if (containsInvalidEnergy)
            return hart::nan<double>();

        std::vector<double> loudness = hart::internal::gateLoudnessEnergies (
            energies,
            -20.0,
            begin->absoluteGateEnergy,
            hart::internal::LoudnessGateBoundary::inclusive
            );

        if (loudness.empty())
            return hart::nan<double>();

        for (double& energy : loudness)
            energy = hart::internal::energyToLufs (energy);

        // Tech 3342's reference selects the observation at the rounded percentile index.
        const double low = hart::percentile (0.10, Interpolation::nearest) (loudness.cbegin(), loudness.cend());
        const double high = hart::percentile (0.95, Interpolation::nearest) (loudness.cbegin(), loudness.cend());
        return high - low;
    }

private:
    hart::internal::LoudnessLayout m_layout;
};

}  // namespace LRA

/// @brief Measures programme Loudness Range (LRA) in LU (Loudness Units)
/// @details Implements [EBU Tech 3342](https://tech.ebu.ch/docs/tech/tech3342.pdf).
///
/// Based on reference MATLAB implementation from EBU Tech 3342.
///
/// Use LRA-specific reducer `LRA::reduce()` for a proper programme reading, instead of
/// generic HART reducers.
///
/// @code
/// lra (audio).get (reduce());  // Entire programme
/// lra (audio).ch (Channel::left).get (LRA::reduce());  // One channel, treated as a mono programme
/// lra (audio).ch ({3, 5}).get (LRA::reduce());  // Selected channels, treated as a stereo programme
/// lra (audio, 0_s).ch (0).get (LRA::reduce());  // No added tail (zero padding)
/// lra (audio).at (slice).get (LRA::reduce (speakerPositions));  // Custom spatial audio setup
/// @endcode
///
/// Three-second windows are observed every 100 ms. Speaker weighting,
/// LFE exclusion and slice-local K-weighting follow the LUFS implementation.
///
/// Supports `Unit::LU` and `Unit::native` (same as LU). `Unit::dB` is not supported,
/// since while it's mathematically same as LU, the standard explicitly uses LU.
///
/// Returns `NaN` for empty analysis ranges, fewer than 3 s of original selected audio,
/// non-finite measured input or no observations surviving the loudness gates.
///
/// For other dynamic range related metrics, see `hart::plr()` and `hart::crestFactor()`.
///
/// @attention LRA is designed for pieces of audio longer than 3 seconds, so you're expected to
/// render enough audio for it to be calculated properly.
/// @param buffer A piece of audio to measure its loudness range.
/// @param tailDurationSeconds Virtual silence processed after the selected slice,
/// defaulting to 1.5 s, as recommended by the reference implementation's file-based
/// measurement note. Zero disables the tail. The original buffer is not modified.
/// @tparam SampleType Type of audio buffer's samples, typically `float` or `double`
/// @ingroup Metrics
template <typename SampleType>
MetricQuery<hart::internal::PerChannelLoudnessData> lra (
    const AudioBuffer<SampleType>& buffer, double tailDurationSeconds = 1.5)
{
    using ValueType = hart::internal::PerChannelLoudnessData;
    typename MetricQuery<ValueType>::SingleChannelMetricEvaluator evaluator =
        [&buffer, tailDurationSeconds] (size_t channel, Slice slice, Unit requestedUnit) -> ValueType
    {
        if (requestedUnit != Unit::native && requestedUnit != Unit::LU)
            HART_THROW_OR_RETURN (hart::UnitError, "LRA supports native and LU units", {});

        if (! std::isfinite (tailDurationSeconds) || tailDurationSeconds < 0.0)
            HART_THROW_OR_RETURN (hart::ValueError, "LRA tail duration must be finite and non-negative", {});

        if (! buffer.hasSampleRate())
            HART_THROW_OR_RETURN (hart::SampleRateError, "LRA requires a sample rate", {});

        const double sampleRateHz = buffer.getSampleRateHz();

        if (! std::isfinite (sampleRateHz) || sampleRateHz <= 2.0 * hart::internal::loudnessShelfFrequencyHz)
            HART_THROW_OR_RETURN (hart::SampleRateError, "Sample rate must exceed twice the K-weighting shelf frequency", {});

        if (slice.isEmpty())
            return {};

        const auto indices = buffer.getFrameIndices (slice);
        const size_t numFrames = indices.second - indices.first;
        const size_t shortTermFrames = static_cast<size_t> (std::round (3.0 * sampleRateHz));

        if (numFrames < shortTermFrames)
            HART_THROW_OR_RETURN (hart::SizeError, "The piece audio is too short", {});

        const double tailFrames = std::round (tailDurationSeconds * sampleRateHz);

        if (! std::isfinite (tailFrames) || tailFrames >= static_cast<double> (std::numeric_limits<size_t>::max() - numFrames))
            HART_THROW_OR_RETURN (hart::ValueError, "LRA tail duration exceeds the supported frame count", {});

        return hart::internal::measureLoudnessEnergies (
            buffer[channel] + indices.first,
            numFrames,
            sampleRateHz,
            1.0,  // energyScale
            static_cast<size_t> (tailFrames)
            );
    };

    const size_t numChannels = buffer.getNumChannels();

    return MetricQuery<ValueType> (
        std::move (evaluator),
        numChannels,
        ChannelSubsets::allChannels (numChannels)
        );
}

}  // namespace hart
