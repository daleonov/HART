#pragma once

#include <utility>
#include <algorithm>

#include "hart_exceptions.hpp"
#include "hart_utils.hpp"  // inf, floatsEqual()

namespace hart
{

/// @brief Represents a slice of analysis data
/// @details
/// Slice stores the user's intent in a raw, domain-specific form.
/// Interpretation happens later inside of containers like AudioBuffer or Spectrum.
///
/// Examples:
/// @code
/// Slice::frames (100, 200)
/// Slice::time (50_ms, 100_ms)
/// Slice::frequency (500_Hz, 1000_Hz)
/// Slice::bins (10, 20)
/// @endcode
/// @ingroup DataStructures
struct Slice
{
    enum class Type
    {
        whole,
        frames,
        time,
        bins,
        frequency
    };

    Type type = Type::whole;
    double start = -hart::inf;
    double stop = hart::inf;

    bool isEmpty () const
    {
        // TODO: Implement unit (type) aware tolerance

        return hart::floatsEqual (start, stop);
    }

    static Slice whole()
    {
        return Slice();
    }

    static Slice frames (size_t startFrame, size_t stopFrame)
    {
        if (startFrame > stopFrame)
            HART_THROW_OR_RETURN (hart::SizeError, "Requested start is greater than its stop", whole());

        Slice slice;
        slice.type = Type::frames;
        slice.start = static_cast<double> (startFrame);
        slice.stop = static_cast<double> (stopFrame);
        return slice;
    }

    static Slice time (double startSeconds, double stopSeconds)
    {
        // If a legitimate case for negative time boundary
        // reveals itself, we can remove this check
        if (startSeconds < 0.0 || stopSeconds < 0.0)
            HART_THROW_OR_RETURN (hart::ValueError, "Slice boundaries should not be negative", whole());

        if (startSeconds > stopSeconds)
            HART_THROW_OR_RETURN (hart::SizeError, "Requested start is greater than its stop", whole());

        Slice slice;
        slice.type = Type::time;
        slice.start = startSeconds;
        slice.stop = stopSeconds;
        return slice;
    }

    static Slice bins (size_t startBin, size_t stopBin)
    {
        if (startBin > stopBin)
            HART_THROW_OR_RETURN (hart::SizeError, "Requested start is greater than its stop", whole());

        Slice slice;
        slice.type = Type::bins;
        slice.start = static_cast<double> (startBin);
        slice.stop = static_cast<double> (stopBin);
        return slice;
    }

    static Slice freq (double startHz, double stopHz)
    {
        if (startHz < 0.0 || stopHz < 0.0)
            HART_THROW_OR_RETURN (hart::ValueError, "Slice boundaries should not be negative", whole());

        if (startHz > stopHz)
            HART_THROW_OR_RETURN (hart::SizeError, "Requested start is greater than its stop", whole());

        Slice slice;
        slice.type = Type::frequency;
        slice.start = startHz;
        slice.stop = stopHz;
        return slice;
    }

private:
    Slice() = default;
};

} // namespace hart
