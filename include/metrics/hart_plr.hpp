#pragma once

#include <algorithm>
#include <cmath>
#include <iterator>
#include <utility>
#include <vector>

#include "hart_audio_buffer.hpp"
#include "hart_exceptions.hpp"
#include "hart_utils.hpp"
#include "metrics/hart_lufs.hpp"
#include "metrics/hart_metric_query.hpp"
#include "metrics/hart_true_peak.hpp"

namespace hart
{
namespace internal
{

struct PerChannelPlrData
{
    PerChannelLoudnessData loudness;
    double truePeakDb = hart::nan<double>();
};

}  // namespace internal

namespace PLR
{

/// @brief Reduces selected channels to programme peak-to-loudness ratio in dB
/// @details Re-uses integrated loudness with inferred or explicit speaker positions.
/// Maximum true peak includes every selected channel, including LFE; the loudness
/// calculation excludes LFE. Positions follow the selected channel order.
/// Returns `NaN` when true peak or integrated loudness has no finite reading.
///
/// If `speakerPositions` are explicitly supplied (for spatial configurations),
/// the vector will be consumed by this reducer.
/// @ingroup Reducers
struct reduce
{
    explicit reduce (std::vector<SpeakerPosition> speakerPositions = {}) :
        m_integrated (std::move (speakerPositions))
    {
    }

    template <typename IteratorType>
    double operator() (IteratorType begin, IteratorType end) const
    {
        std::vector<hart::internal::PerChannelLoudnessData> loudness;
        loudness.reserve (static_cast<size_t> (std::distance (begin, end)));
        double maximumTruePeakDb = -hart::inf;

        for (IteratorType it = begin; it != end; ++it)
        {
            if (std::isnan (it->truePeakDb))
                return hart::nan<double>();

            maximumTruePeakDb = std::max (maximumTruePeakDb, it->truePeakDb);
            loudness.push_back (it->loudness);
        }

        const double integratedLufs = m_integrated (loudness.cbegin(), loudness.cend());

        if (! std::isfinite (maximumTruePeakDb) || ! std::isfinite (integratedLufs))
            return hart::nan<double>();

        // PLR = TP - LUFS-I
        return maximumTruePeakDb - integratedLufs;
    }

private:
    LUFS::integrated m_integrated;
};

}  // namespace PLR

/// @brief Calculates programme peak-to-loudness ratio (PLR) in decibels.
/// @details
/// Defined in [AES Technical Document AESTD1008.1.21-9](https://aes.org/community/technical-council/technical-document-aestd1008/),
/// Annex A, page 19:
///
/// PLR = TP - LUFS-I
///
/// where TP is the maximum true-peak level in dBTP and LUFS-I is the integrated
/// BS.1770 loudness in LUFS, measured over the same selected channels and slice.
/// The resulting difference is in dB.
///
/// Use the metric-specific reducer `PLR::reduce()` to calculate programme PLR
/// over the selected channels and slice, instead of using generic HART reducers.
/// The reason is that raw results of this metric do not contain per-channel
/// PLRs, since it's a program-wide metric, rather than per-channel one.
///
/// Supports `Unit::dB` and `Unit::native` (same as dB).
///
/// Speaker layout inference, custom positions and loudness gating are inherited
/// from `LUFS::integrated()`. True peak uses `truePeak()` with its default settings.
/// Silence, empty analysis ranges and wholly gated-out programmes return `NaN`.
/// @code
/// plr (audio).get (PLR::reduce());
/// plr (audio).ch ({ 0, 1 }).get (PLR::reduce());
/// plr (audio).at (slice).get (PLR::reduce (speakerPositions));
/// @endcode
///
/// For a more simple dynamic range metric, see `hart::crestFactor()`. Also, see
/// `hart::lra()` for another high-level dynamic range metric.
/// @returns A chainable metric query object containing raw per-channel PLR data.
/// @ingroup Metrics
template <typename SampleType>
MetricQuery<hart::internal::PerChannelPlrData> plr (const AudioBuffer<SampleType>& buffer)
{
    using ValueType = hart::internal::PerChannelPlrData;
    const auto loudnessQuery = lufs (buffer);
    const auto peakQuery = truePeak (buffer).as (Unit::dB);

    typename MetricQuery<ValueType>::SingleChannelMetricEvaluator evaluator =
        [loudnessQuery, peakQuery] (size_t channel, Slice slice, Unit requestedUnit) -> ValueType
    {
        if (requestedUnit != Unit::native && requestedUnit != Unit::dB)
            HART_THROW_OR_RETURN (hart::UnitError, "PLR supports native and dB units", {});

        ValueType result;
        result.loudness = loudnessQuery.ch (channel).at (slice).get();
        if (result.loudness.numFrames == 0)
            return result;

        result.truePeakDb = peakQuery.ch (channel).at (slice).get();
        return result;
    };

    const size_t numChannels = buffer.getNumChannels();
    return MetricQuery<ValueType> (
        std::move (evaluator),
        numChannels,
        ChannelSubsets::allChannels (numChannels)
        );
}

}  // namespace hart
