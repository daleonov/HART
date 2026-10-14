#pragma once

#include <utility>
#include <vector>

#include "hart_audio_buffer.hpp"
#include "hart_reducers.hpp"
#include "metrics/hart_loudness_common.hpp"
#include "metrics/hart_metric_query.hpp"
#include "metrics/hart_metrics_common.hpp"

namespace hart
{
namespace LUFS
{

/// @brief A reducer to obtail LUFS-I value
/// @details Integrates programme loudness with BS.1770 absolute and relative gates.
/// This is LUFS-specific reducer, meant to be used with `lufs()` metric.
/// Infers mono, stereo, 5.0 or 5.1 from selected channel count, unless
/// positions are supplied in selected channel order. LFE is excluded.
/// @ingroup Reducers
struct integrated
{
    integrated() = default;

    explicit integrated (std::vector<SpeakerPosition> speakerPositions) :
        m_speakerPositions (std::move (speakerPositions))
    {
    }

    template <typename IteratorType>
    double operator() (IteratorType begin, IteratorType end) const
    {
        const std::vector<double> energies = hart::internal::combineLoudnessEnergies (
            begin,
            end,
            hart::internal::LoudnessWindow::integrated,
            m_speakerPositions
            );

        if (begin == end)
            return hart::nan<double>();

        return hart::internal::integratedLufs (energies, begin->absoluteGateEnergy);
    }

private:
    hart::internal::LoudnessLayout m_speakerPositions;
};

/// @private
namespace internal
{

template <typename TemporalReducer>
class WindowedLoudnessReducer
{
public:
    WindowedLoudnessReducer (
        hart::internal::LoudnessWindow window,
        TemporalReducer reducer,
        hart::internal::LoudnessLayout layout
        ) :
        m_window (window),
        m_reducer (std::move (reducer)),
        m_layout (std::move (layout))
    {
    }

    template <typename IteratorType>
    double operator() (IteratorType begin, IteratorType end) const
    {
        std::vector<double> energies = hart::internal::combineLoudnessEnergies (begin, end, m_window, m_layout);
    
        if (energies.empty())
            return hart::nan<double>();

        for (double& energy : energies)
        {
            if (std::isnan (energy))
                return hart::nan<double>();

            energy = hart::internal::energyToLufs (energy);
        }

        return m_reducer (energies.cbegin(), energies.cend());
    }

private:
    const hart::internal::LoudnessWindow m_window;
    TemporalReducer m_reducer;
    hart::internal::LoudnessLayout m_layout;
};

}  // namespace internal

/// @brief A reducer to obtail LUFS-S value
/// @details Reduces ungated 3 s programme loudness observations, sampled every 100 ms.
/// This is LUFS-specific reducer, meant to be used with `lufs()` metric.
/// Defaults to the maximum reading. Empty positions infer the layout from channel count.
/// Custom positions follow selected channel order.
/// @ingroup Reducers
template <typename TemporalReducer = hart::max>
internal::WindowedLoudnessReducer<TemporalReducer> shortTerm (
    TemporalReducer reducer = TemporalReducer(),
    std::vector<SpeakerPosition> speakerPositions = {}
    )
{
    return {
        hart::internal::LoudnessWindow::shortTerm, std::move (reducer),
        hart::internal::LoudnessLayout (std::move (speakerPositions))
        };
}

/// @brief A reducer to obtail LUFS-M value
/// @details Reduces ungated 400 ms programme loudness observations, sampled every 100 ms.
/// This is LUFS-specific reducer, meant to be used with `lufs()` metric.
/// Defaults to the maximum reading. Empty positions infer the layout from channel count.
/// Custom positions follow selected channel order.
/// @ingroup Reducers
template <typename TemporalReducer = hart::max>
internal::WindowedLoudnessReducer<TemporalReducer> momentary (
    TemporalReducer reducer = TemporalReducer(),
    std::vector<SpeakerPosition> speakerPositions = {}
    )
{
    return {
        hart::internal::LoudnessWindow::momentary, std::move (reducer),
        hart::internal::LoudnessLayout (std::move (speakerPositions))
        };
}

}  // namespace LUFS

/// @brief Estimates LUFS loudness of the signal.
/// @details Based on [ITU-R BS.1770-5](https://www.itu.int/rec/R-REC-BS.1770-5-202311-I/en)
/// and [EBU Tech 3341](https://tech.ebu.ch/docs/tech/tech3341.pdf).
/// Generated reference tests meet the numerical expectations of Tech 3341 cases
/// #1-6 and #9-12 within 0.1 LU; case #11 is checked through offline segment measurements.
/// Full EBU Mode compliance is not claimed: momentary maxima are currently sampled
/// every 100 ms and can under-read brief tones, as exposed by case #13.
///
/// To obtain integrated, short-term or momentary readings, use the LUFS-specific
/// reducers below instead of generic reducers. Short-term and momentary default to
/// the maximum reading; an explicit temporal reducer can select another summary.
/// Generic reduce2d() reducers operate on linear window energies, not LUFS readings.
///
/// @code
/// lufs (audio).get (LUFS::integrated());  // Integrated programme loudness
/// lufs (audio).get (LUFS::momentary());  // Maximum momentary loudness
/// lufs (audio).at (Slice::time (1_s, 4_s)).get (LUFS::shortTerm());  // Maximum short-term loudness of a slice
/// lufs (audio).ch (0).get (LUFS::integrated());  // Mono reading of one selected channel
/// lufs (audio).get (LUFS::shortTerm (percentile (0.5)));  // Median short-term loudness
/// lufs (audio).as (LU).get (LUFS::integrated());  // Loudness relative to the EBU -23 LUFS target (in LU)
/// @endcode
///
/// Supports `Unit::native` and `Unit::LUFS` (default) for absolute loudness, and `Unit::LU`
/// for loudness relative to -23 LUFS (LU = LUFS + 23). `Unit::dB` is not supported, to avoid
/// ambiguity between LUFS and LU readings.
///
/// Selected channel counts infer mono, stereo, 5.0 or 5.1; other speaker layouts
/// require explicit positions passed to the reducer in the selected channel order.
/// LFE channels are excluded. Filter state resets at the start of the selected slice.
///
/// Silence and integrated measurements with no blocks above the gates return `-inf`.
/// Empty analysis ranges and non-finite measured input return `NaN`. Momentary and
/// short-term also return `NaN` when less than 400 ms or 3 s of audio is available,
/// respectively. For non-empty clips shorter than 400 ms, integrated uses a single
/// available-length block; this is a HART extension to the standard measurement.
/// @returns A chainable metric query object
/// @ingroup Metrics
template <typename SampleType>
MetricQuery<hart::internal::PerChannelLoudnessData> lufs (const AudioBuffer<SampleType>& buffer)
{
    typename MetricQuery<hart::internal::PerChannelLoudnessData>::SingleChannelMetricEvaluator evaluator =
        [&buffer] (size_t channel, Slice slice, Unit requestedUnit) -> hart::internal::PerChannelLoudnessData
    {
        hassert (channel < buffer.getNumChannels());

        if (requestedUnit != Unit::native && requestedUnit != Unit::LUFS && requestedUnit != Unit::LU)
            HART_THROW_OR_RETURN (hart::UnitError, "LUFS supports native, LUFS and LU units", {});

        if (! buffer.hasSampleRate())
            HART_THROW_OR_RETURN (hart::SampleRateError, "LUFS requires a sample rate", {});

        const double sampleRateHz = buffer.getSampleRateHz();

        if (! std::isfinite (sampleRateHz) || sampleRateHz <= 2.0 * hart::internal::loudnessShelfFrequencyHz)
            HART_THROW_OR_RETURN (hart::SampleRateError,
                "Sample rate must exceed twice the K-weighting shelf frequency", {});

        if (slice.isEmpty())
            return {};

        const auto indices = buffer.getFrameIndices (slice);

        if (indices.first == indices.second)
            return {};

        // Apply the LU reference before reduction; the absolute gate scales with it.
        const double energyScale = requestedUnit == Unit::LU
            ? std::pow (10.0, -hart::internal::ebuTargetLufs / 10.0)
            : 1.0;

        return hart::internal::measureLoudnessEnergies (
            buffer[channel] + indices.first,
            indices.second - indices.first,
            sampleRateHz, energyScale
            );
    };

    const size_t numChannels = buffer.getNumChannels();

    return MetricQuery<hart::internal::PerChannelLoudnessData> (
        std::move (evaluator),
        numChannels,
        ChannelSubsets::allChannels (numChannels)
        );
}

}  // namespace hart
