#pragma once

#include <algorithm>
#include <cmath>
#include <iterator>
#include <utility>
#include <vector>

#include "hart_accurate_sum.hpp"
#include "hart_exceptions.hpp"
#include "hart_units.hpp"
#include "hart_utils.hpp"

namespace hart
{
/// @private
namespace internal
{

class BiquadFilter
{
public:
    BiquadFilter (double b0, double b1, double b2, double a1, double a2) :
        m_b0 (b0), m_b1 (b1), m_b2 (b2), m_a1 (a1), m_a2 (a2)
    {}

    double processSample (double input)
    {
        const double output = m_b0 * input + m_z1;
        m_z1 = m_b1 * input - m_a1 * output + m_z2;
        m_z2 = m_b2 * input - m_a2 * output;
        return output;
    }

private:
    double m_b0, m_b1, m_b2, m_a1, m_a2;
    double m_z1 = 0.0;
    double m_z2 = 0.0;
};

// Prototype parameters reconstruct ITU-R BS.1770-5, Annex 1, Table 1 (p. 4)
// and Table 2 (p. 5), which specify digital coefficients at 48 kHz.
// These are derived prototype parameters, not frequencies printed in the standard.
// The same parameterization is used in FFmpeg's ebur128_init_filter():
// https://github.com/FFmpeg/FFmpeg/blob/master/libavfilter/ebur128.c
// Standard: https://www.itu.int/rec/R-REC-BS.1770-5-202311-I/en
constexpr double loudnessShelfFrequencyHz = 1681.974450955533;
constexpr double loudnessShelfQualityFactor = 0.7071752369554196;
constexpr double loudnessShelfGainDb = 3.999843853973347;
constexpr double loudnessShelfIntermediateGainExponent = 0.4996667741545416;
constexpr double loudnessRlbFrequencyHz = 38.13547087602444;
constexpr double loudnessRlbQualityFactor = 0.5003270373238773;

// BS.1770-5, Annex 1, equation (2) and the absolute gate in equation (5).
constexpr double loudnessCalibrationDb = -0.691;
constexpr double loudnessAbsoluteGateLufs = -70.0;
constexpr double ebuTargetLufs = -23.0;

inline double absoluteLoudnessGateEnergy()
{
    return std::pow (10.0, (loudnessAbsoluteGateLufs - loudnessCalibrationDb) / 10.0);
}

inline BiquadFilter makeLoudnessPreFilter (double sampleRateHz)
{
    const double k = std::tan (hart::pi * loudnessShelfFrequencyHz / sampleRateHz);
    const double q = loudnessShelfQualityFactor;
    const double vh = std::pow (10.0, loudnessShelfGainDb / 20.0);
    const double vb = std::pow (vh, loudnessShelfIntermediateGainExponent);
    const double kSquared = k * k;
    const double a0 = 1.0 + k / q + kSquared;

    return BiquadFilter (
        (vh + vb * k / q + kSquared) / a0,
        2.0 * (kSquared - vh) / a0,
        (vh - vb * k / q + kSquared) / a0,
        2.0 * (kSquared - 1.0) / a0,
        (1.0 - k / q + kSquared) / a0
    );
}

inline BiquadFilter makeLoudnessRlbFilter (double sampleRateHz)
{
    const double k = std::tan (hart::pi * loudnessRlbFrequencyHz / sampleRateHz);
    const double q = loudnessRlbQualityFactor;
    const double kSquared = k * k;
    const double a0 = 1.0 + k / q + kSquared;

    // BS.1770's RLB numerator is deliberately not normalized by a0.
    return BiquadFilter (
        1.0, -2.0, 1.0,
        2.0 * (kSquared - 1.0) / a0,
        (1.0 - k / q + kSquared) / a0
    );
}

inline double energyToLufs (double energy)
{
    return energy <= 0.0
        ? -hart::inf
        : loudnessCalibrationDb + 10.0 * std::log10 (energy);
}

// Energies stay linear; LU requests scale energies and the absolute gate together.
// Iteration exposes the 400 ms energy series to generic 2D reducers.
struct PerChannelLoudnessData
{
    std::vector<double> momentaryEnergies;
    std::vector<double> shortTermEnergies;
    // In the same linear calibration as the energy vectors.
    double absoluteGateEnergy = absoluteLoudnessGateEnergy();
    size_t numFrames = 0;
    size_t momentaryFrames = 0;
    size_t shortTermFrames = 0;
    size_t hopFrames = 0;
    double sampleRateHz = 0.0;

    std::vector<double>::const_iterator begin() const { return momentaryEnergies.begin(); }
    std::vector<double>::const_iterator end() const { return momentaryEnergies.end(); }
};

class LoudnessEnergyWindow
{
public:
    LoudnessEnergyWindow (size_t windowFrames, size_t hopFrames, size_t numFrames) :
        m_history (windowFrames, 0.0), m_hopFrames (hopFrames)
    {
        if (numFrames >= windowFrames)
            m_energies.reserve (1 + (numFrames - windowFrames) / hopFrames);
    }

    void push (double squaredSample, size_t frame)
    {
        const size_t index = frame % m_history.size();
        m_sum += -m_history[index];
        m_sum += squaredSample;
        m_history[index] = squaredSample;
        const size_t elapsed = frame + 1;
        if (elapsed >= m_history.size() && (elapsed - m_history.size()) % m_hopFrames == 0)
            m_energies.push_back (std::max (0.0, m_sum.getValue()) / m_history.size());
    }

    std::vector<double> finish (size_t numFrames)
    {
        // This partial block is used only by integrated loudness for short clips.
        if (m_energies.empty() && numFrames != 0)
            m_energies.push_back (std::max (0.0, m_sum.getValue()) / numFrames);
        return std::move (m_energies);
    }

private:
    std::vector<double> m_history;
    const size_t m_hopFrames;
    AccurateSum<double> m_sum;
    std::vector<double> m_energies;
};

template <typename SampleType>
PerChannelLoudnessData measureLoudnessEnergies (const SampleType* input, size_t numFrames,
    double sampleRateHz, double energyScale)
{
    PerChannelLoudnessData result;
    result.absoluteGateEnergy *= energyScale;
    result.numFrames = numFrames;
    result.sampleRateHz = sampleRateHz;
    result.momentaryFrames = static_cast<size_t> (std::round (0.4 * sampleRateHz));
    result.shortTermFrames = static_cast<size_t> (std::round (3.0 * sampleRateHz));
    result.hopFrames = static_cast<size_t> (std::round (0.1 * sampleRateHz));
    LoudnessEnergyWindow momentary (result.momentaryFrames, result.hopFrames, numFrames);
    LoudnessEnergyWindow shortTerm (result.shortTermFrames, result.hopFrames, numFrames);
    BiquadFilter preFilter = makeLoudnessPreFilter (sampleRateHz);
    BiquadFilter rlbFilter = makeLoudnessRlbFilter (sampleRateHz);

    for (size_t frame = 0; frame < numFrames; ++frame)
    {
        const double sample = static_cast<double> (input[frame]);
        const double filtered = rlbFilter.processSample (preFilter.processSample (sample));
        const double squared = filtered * filtered * energyScale;
        if (! std::isfinite (sample) || ! std::isfinite (squared))
        {
            result.momentaryEnergies = { hart::nan<double>() };
            result.shortTermEnergies = { hart::nan<double>() };
            return result;
        }
        momentary.push (squared, frame);
        shortTerm.push (squared, frame);
    }

    result.momentaryEnergies = momentary.finish (numFrames);
    result.shortTermEnergies = shortTerm.finish (numFrames);
    return result;
}

// Reusable by loudness-derived metrics; a position is not itself a loudness weight.
class LoudnessLayout
{
public:
    LoudnessLayout() = default;
    explicit LoudnessLayout (std::vector<SpeakerPosition> positions) :
        m_positions (std::move (positions))
    {}

    std::vector<double> weights (size_t numChannels) const
    {
        if (m_positions.empty())
        {
            switch (numChannels)
            {
                case 1: return { 1.0 };
                case 2: return { 1.0, 1.0 };
                case 5: return { 1.0, 1.0, 1.0, 1.41, 1.41 };
                case 6: return { 1.0, 1.0, 1.0, 0.0, 1.41, 1.41 };
                default:
                    HART_THROW_OR_RETURN (hart::ChannelLayoutError,
                        "LUFS requires mono, stereo, 5.0, 5.1, or explicit speaker positions", {});
            }
        }

        if (m_positions.size() != numChannels)
            HART_THROW_OR_RETURN (hart::ChannelLayoutError,
                "Speaker position count must match the selected channels", {});

        std::vector<double> result;
        result.reserve (numChannels);
        for (const SpeakerPosition& speaker : m_positions)
        {
            if (! std::isfinite (speaker.azimuthRadians) || std::abs (speaker.azimuthRadians) > hart::pi)
                HART_THROW_OR_RETURN (hart::ValueError, "Invalid speaker azimuth position", {});

            if (! std::isfinite (speaker.elevationRadians) || std::abs (speaker.elevationRadians) > hart::halfPi)
                HART_THROW_OR_RETURN (hart::ValueError, "Invalid speaker elevation", {});

            // ITU-R BS.1770-5, Annex 3, Table 4. Boundaries are inclusive in azimuth.
            const double azimuth = std::abs (speaker.azimuthRadians);
            const bool isLateral =
                azimuth >= hart::pi / 3.0
                && azimuth <= 2.0 * hart::pi / 3.0
                && std::abs (speaker.elevationRadians) < hart::pi / 6.0;

            result.push_back (speaker.isLfe == LFE::yes ? 0.0 : (isLateral ? 1.41 : 1.0));
        }
        return result;
    }

private:
    std::vector<SpeakerPosition> m_positions;
};

enum class LoudnessWindow { integrated, momentary, shortTerm };

// Sum energies before any gate or temporal reducer, respecting selected channel order.
template <typename IteratorType>
std::vector<double> combineLoudnessEnergies (IteratorType begin, IteratorType end,
    LoudnessWindow window, const LoudnessLayout& layout)
{
    if (begin == end)
        HART_THROW_OR_RETURN (hart::SizeError, "No channels selected for loudness", {});

    const std::vector<double> weights = layout.weights (static_cast<size_t> (std::distance (begin, end)));

    if (weights.empty())
        return { hart::nan<double>() };

    const PerChannelLoudnessData& first = *begin;
    const bool shortTerm = window == LoudnessWindow::shortTerm;

    if (first.numFrames == 0)
        return { hart::nan<double>() };

    const bool incompleteMomentaryWindow = window == LoudnessWindow::momentary
        && first.numFrames < first.momentaryFrames;
    if (incompleteMomentaryWindow)
        return { hart::nan<double>() };

    const bool incompleteShortTermWindow = shortTerm && first.numFrames < first.shortTermFrames;
    if (incompleteShortTermWindow)
        return { hart::nan<double>() };

    const size_t windowFrames = shortTerm ? first.shortTermFrames : first.momentaryFrames;
    const size_t numWindows =
        first.numFrames < windowFrames
            ? 1
            : 1 + (first.numFrames - windowFrames) / first.hopFrames;

    std::vector<AccurateSum<double>> sums (numWindows);
    size_t channel = 0;

    for (IteratorType it = begin; it != end; ++it, ++channel)
    {
        const PerChannelLoudnessData& value = *it;
        const std::vector<double>& energies = shortTerm ? value.shortTermEnergies : value.momentaryEnergies;

        if (value.numFrames != first.numFrames)
            HART_THROW_OR_RETURN (hart::ValueError, "Loudness measurements must have equal frame counts", {});

        if (value.sampleRateHz != first.sampleRateHz)
            HART_THROW_OR_RETURN (hart::ValueError, "Loudness measurements must have equal sample rates", {});

        const bool matchingWindowTiming = value.hopFrames == first.hopFrames
            && value.momentaryFrames == first.momentaryFrames && value.shortTermFrames == first.shortTermFrames;
        if (! matchingWindowTiming)
            HART_THROW_OR_RETURN (hart::ValueError, "Loudness measurements must have aligned windows", {});

        if (value.absoluteGateEnergy != first.absoluteGateEnergy)
            HART_THROW_OR_RETURN (hart::ValueError, "Loudness measurements must have equal energy calibration", {});

        if (weights[channel] == 0.0)
            continue;

        // Invalid input produces one NaN rather than a full series.
        if (energies.size() == 1 && std::isnan (energies.front()))
            return { hart::nan<double>() };

        if (energies.size() != numWindows)
            HART_THROW_OR_RETURN (hart::SizeError, "Loudness window counts must match", {});

        for (size_t index = 0; index < numWindows; ++index)
        {
            if (! std::isfinite (energies[index]) || energies[index] < 0.0)
                return { hart::nan<double>() };

            sums[index] += weights[channel] * energies[index];
        }
    }

    std::vector<double> result;
    result.reserve (numWindows);

    for (const AccurateSum<double>& sum : sums)
        result.push_back (sum.getValue());

    return result;
}

// Share gating mechanics with LRA without sharing its different relative threshold.
inline std::vector<double> gateLoudnessEnergies (const std::vector<double>& energies, double relativeGateLu,
    double absoluteGateEnergy = absoluteLoudnessGateEnergy())
{
    AccurateSum<double> absoluteSum;
    size_t absoluteCount = 0;

    for (double energy : energies)
    {
        if (energy > absoluteGateEnergy)
        {
            absoluteSum += energy;
            ++absoluteCount;
        }
    }

    if (absoluteCount == 0)
        return {};

    const double relativeGateEnergy = std::pow (10.0, relativeGateLu / 10.0)
        * absoluteSum.getValue() / static_cast<double> (absoluteCount);

    std::vector<double> result;

    for (double energy : energies)
        if (energy > absoluteGateEnergy && energy > relativeGateEnergy)
            result.push_back (energy);

    return result;
}

inline double integratedLufs (const std::vector<double>& energies,
    double absoluteGateEnergy = absoluteLoudnessGateEnergy())
{
    if (energies.empty())
        return hart::nan<double>();

    const bool containsInvalidEnergy = std::any_of (energies.begin(), energies.end(),
        [] (double energy) { return ! std::isfinite (energy); });
    if (containsInvalidEnergy)
        return hart::nan<double>();

    const std::vector<double> gated = gateLoudnessEnergies (energies, -10.0, absoluteGateEnergy);

    if (gated.empty())
        return -hart::inf;

    AccurateSum<double> sum;

    for (double energy : gated)
        sum += energy;

    return energyToLufs (sum.getValue() / static_cast<double> (gated.size()));
}

}  // namespace internal
}  // namespace hart
