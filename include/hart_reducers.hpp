#pragma once

#include <algorithm>
#include <cmath>
#include <iterator>
#include <vector>

#include "hart_accurate_sum.hpp"
#include "hart_exceptions.hpp"
#include "metrics/hart_metrics_common.hpp"  // ReducerResultType
#include "hart_utils.hpp"  // nan(), floatsNotEqual(), Interpolation, Axis

namespace hart
{

/// @defgroup Reducers Reducers
/// @brief Helper functions to reduce multi-channel metrics to a single scalar
/// @{

/// @brief Value of the elements in a range that reducer is supposed to reduce
template <typename IteratorType>
using IteratedValueType =
    typename std::iterator_traits<IteratorType>::value_type;

/// @brief Returns `true` if all the values in the range are equal to each other within provided tolerance, `false` otherwise
struct allFloatsEqualToEachOther
{
    allFloatsEqualToEachOther (double toleranceLinear = 1e-8) :
        m_toleranceLinear (toleranceLinear)
    {}

    template <typename IteratorType>
    bool operator() (IteratorType begin, IteratorType end) const
    {
        if (begin == end)
            return true;

        const auto toleranceLinear = static_cast<IteratedValueType<IteratorType>> (m_toleranceLinear);
        const IteratedValueType<IteratorType> referenceValue = *begin;
        ++begin;

        for (IteratorType it = begin; it != end; ++it)
            if (floatsNotEqual (*it, referenceValue, toleranceLinear))
                return false;

        return true;
    }
private:
    const double m_toleranceLinear;
};

/// @brief Returns `true` if all the values in the range are equal to a specific value within provided tolerance, `false` otherwise
struct allFloatsEqualTo
{
    allFloatsEqualTo (double targetValue, double toleranceLinear = 1e-8) :
        m_targetValue (targetValue),
        m_toleranceLinear (toleranceLinear)
    {}

    template <typename IteratorType>
    bool operator() (IteratorType begin, IteratorType end) const
    {
        if (begin == end)
            return true;

        const auto targetValue = static_cast<IteratedValueType<IteratorType>> (m_targetValue);
        const auto toleranceLinear = static_cast<IteratedValueType<IteratorType>> (m_toleranceLinear);

        for (IteratorType it = begin; it != end; ++it)
            if (floatsNotEqual (*it, targetValue, toleranceLinear))
                return false;

        return true;
    }
private:
    const double m_targetValue;
    const double m_toleranceLinear;
};

/// @brief Returns `true` if at least one element in the range is `NaN`
/// @details Empty ranges return `false`
struct anyNaN
{
    template <typename IteratorType>
    bool operator() (IteratorType begin, IteratorType end) const
    {
        for (IteratorType it = begin; it != end; ++it)
            if (std::isnan (*it))
                return true;

        return false;
    }
};

/// @brief Returns `true` if all elements in the range are `NaN`
/// @details Empty ranges return `false`
struct allNaN
{
    template <typename IteratorType>
    bool operator() (IteratorType begin, IteratorType end) const
    {
        if (begin == end)
            return false;

        for (IteratorType it = begin; it != end; ++it)
            if (! std::isnan (*it))
                return false;

        return true;
    }
};

/// @brief Returns the zero-based index of the largest element in the range
/// @details The returned index is relative to the provided iterator range, not the original container
struct argmax
{
    template <typename IteratorType>
    size_t operator() (IteratorType begin, IteratorType end) const
    {
        if (begin == end)
            HART_THROW_OR_RETURN (hart::SizeError, "The range is empty", 0);

        return static_cast<size_t> (std::distance (begin, std::max_element (begin, end)));
    }
};

/// @brief Returns the zero-based index of the smallest element in the range
/// @details The returned index is relative to the provided iterator range, not the original container
struct argmin
{
    template <typename IteratorType>
    size_t operator() (IteratorType begin, IteratorType end) const
    {
        if (begin == end)
            HART_THROW_OR_RETURN (hart::SizeError, "The range is empty", 0);

        return static_cast<size_t> (std::distance (begin, std::min_element (begin, end)));
    }
};

/// @brief Forwards the entire range as an `std::vector`, preserving the original order
struct collect
{
    template <typename IteratorType>
    std::vector<IteratedValueType<IteratorType>>
    operator() (IteratorType begin, IteratorType end) const
    {
        return std::vector<IteratedValueType<IteratorType>> (begin, end);
    }
};

/// @brief Returns the first element in the range
struct first
{
    template <typename IteratorType>
    IteratedValueType<IteratorType>
    operator() (IteratorType begin, IteratorType end) const
    {
        if (begin == end)
            HART_THROW_OR_RETURN (hart::SizeError, "The range is empty", hart::nan<IteratedValueType<IteratorType>>());

        return *begin;
    }
};

/// @brief Returns the last element in the range
/// @details Requires a bidirectional iterator
struct last
{
    template <typename IteratorType>
    IteratedValueType<IteratorType>
    operator() (IteratorType begin, IteratorType end) const
    {
        if (begin == end)
            HART_THROW_OR_RETURN (hart::SizeError, "The range is empty", hart::nan<IteratedValueType<IteratorType>>());

        return *(--end);
    }
};

/// @brief Returns the largest element in the range
struct max
{
    template <typename IteratorType>
    IteratedValueType<IteratorType>
    operator() (IteratorType begin, IteratorType end) const
    {
        if (begin == end)
            HART_THROW_OR_RETURN (hart::SizeError, "The range is empty", hart::nan<IteratedValueType<IteratorType>>());

        return *std::max_element (begin, end);
    }
};

/// @brief Returns the arithmetic mean of all elements in the range
struct mean
{
    template <typename IteratorType>
    IteratedValueType<IteratorType>
    operator() (IteratorType begin, IteratorType end) const
    {
        if (begin == end)
            HART_THROW_OR_RETURN (hart::SizeError, "The range is empty", hart::nan<IteratedValueType<IteratorType>>());

        AccurateSum<IteratedValueType<IteratorType>> accurateSum;
        size_t count = 0;

        for (IteratorType it = begin; it != end; ++it)
        {
            accurateSum += *it;
            ++count;
        }

        return accurateSum.template get<IteratedValueType<IteratorType>>() / count;
    }
};

/// @brief Returns the smallest element in the range
struct min
{
    template <typename IteratorType>
    IteratedValueType<IteratorType>
    operator() (IteratorType begin, IteratorType end) const
    {
        if (begin == end)
            HART_THROW_OR_RETURN (hart::SizeError, "The range is empty", hart::nan<IteratedValueType<IteratorType>>());

        return *std::min_element (begin, end);
    }
};

/// @brief Returns the nth element in the range (zero-based)
/// @throws hart::IndexError if @p targetIndex is out of range
/// @throws hart::SizeError is the range (or container) is empty
struct nth
{
    nth (size_t targetIndex) : n_targetIndex (targetIndex) {}

    template <typename IteratorType>
    IteratedValueType<IteratorType>
    operator() (IteratorType begin, IteratorType end) const
    {
        if (begin == end)
            HART_THROW_OR_RETURN (hart::SizeError, "The range is empty", hart::nan<IteratedValueType<IteratorType>>());

        IteratorType iterator = begin;

        for (size_t i = 0; i < n_targetIndex; ++i)
        {
            ++iterator;

            if (iterator == end)
                HART_THROW_OR_RETURN (hart::IndexError, "Target index is out of range", hart::nan<IteratedValueType<IteratorType>>());
        }

        return *iterator;
    }
private:
    const size_t n_targetIndex; 
};

/// @brief Returns the percentile value
/// @param quantile Quantile value, in 0..1 range
/// @param interpolation Ceiling (the backwards-compatible default), nearest, or linear
struct percentile
{
    percentile (double quantile, Interpolation interpolation = Interpolation::ceiling) :
        m_quantile (quantile),
        m_interpolation (interpolation)
    {
    }

    template <typename IteratorType>
    auto operator() (IteratorType begin, IteratorType end) const
        -> IteratedValueType<IteratorType>
    {
        hassert (m_interpolation == Interpolation::ceiling || m_interpolation == Interpolation::nearest
            || m_interpolation == Interpolation::linear)

        using ValueType = IteratedValueType<IteratorType>;

        if (! std::isfinite (m_quantile) || m_quantile < 0 || m_quantile > 1)
            HART_THROW_OR_RETURN (hart::ValueError, "Quantile value should be in 0..1 range", hart::nan<ValueType>());

        if (begin == end)
            HART_THROW_OR_RETURN (hart::SizeError, "The range is empty", hart::nan<ValueType>());

        const size_t n = static_cast<size_t> (std::distance (begin, end));
        std::vector<ValueType> data (begin, end);

        if (m_interpolation != Interpolation::linear)
        {
            const double position = m_quantile * (n - 1);
            const size_t index = static_cast<size_t> (m_interpolation == Interpolation::ceiling
                ? std::ceil (position) : std::round (position));

            std::nth_element (data.begin(), data.begin() + index, data.end());
            return data[index];
        }
        else
        {
            // Assuming Interpolation::linear
            // Get a value with a fractional index
            const double pos = m_quantile * (n - 1);
            const size_t i0 = static_cast<size_t> (std::floor (pos));
            const size_t i1 = static_cast<size_t> (std::ceil (pos));
            const double t = pos - i0;

            std::nth_element (data.begin(), data.begin() + i0, data.end());
            const ValueType v0 = data[i0];

            if (i0 == i1)
                return v0;

            std::nth_element (data.begin(), data.begin() + i1, data.end());
            const ValueType v1 = data[i1];

            return (v0 + (v1 - v0) * static_cast<ValueType> (t));
        }
    }

private:
    const double m_quantile;
    const Interpolation m_interpolation;
};

/// @brief Returns the difference between largest and smallest values in the range
struct range
{
    template <typename IteratorType>
    IteratedValueType<IteratorType>
    operator() (IteratorType begin, IteratorType end) const
    {
        if (begin == end)
            HART_THROW_OR_RETURN (hart::SizeError, "The range is empty", hart::nan<IteratedValueType<IteratorType>>());

        const auto largestValue = *std::max_element (begin, end);
        const auto smallestValue = *std::min_element (begin, end);
        return largestValue - smallestValue;
    }
};

/// @private
template <typename ReducerAType, typename ReducerBType>
struct Reduce2D
{
    Reduce2D (ReducerAType reducerA, ReducerBType reducerB, Axis axis) :
        m_reducerA (reducerA),
        m_reducerB (reducerB),
        m_axis (axis)
    {
    }

    template <typename ChannelIteratorType>
    auto operator() (ChannelIteratorType begin, ChannelIteratorType end) const
        -> ReducerResultType<
        ReducerAType,
        typename std::vector<
            ReducerResultType<ReducerBType, decltype ((*std::declval<ChannelIteratorType>()).begin())>
            >::const_iterator
        >
    {
        typedef decltype ((*std::declval<ChannelIteratorType>()).begin()) FrameIteratorType;
        typedef ReducerResultType<ReducerBType, FrameIteratorType> ResultBType;

        std::vector<ResultBType> reducerBResults;

        if (m_axis == Axis::channels)
        {
            reducerBResults.reserve (static_cast<size_t> (std::distance (begin, end)));

            for (ChannelIteratorType channel = begin; channel != end; ++channel)
            {
                reducerBResults.push_back (
                    m_reducerB (
                        (*channel).begin(),
                        (*channel).end()
                    )
                );
            }

            return m_reducerA (reducerBResults.begin(), reducerBResults.end());
        }

        // Else: m_axis == Axis::frames
        const size_t numFrames =
            begin == end
                ? 0
                : static_cast<size_t> (std::distance ((*begin).begin(), (*begin).end()));

        for (ChannelIteratorType channel = begin; channel != end; ++channel)
        {
            if (static_cast<size_t> (std::distance ((*channel).begin(), (*channel).end())) != numFrames)
            {
                // There can be legitimate cases where the input is not an evenly sized 2D vector,
                // so if you're triggering this exception, it might be a sign that it's time
                // to get reducer2d() to properly support those.
                HART_THROW_OR_RETURN (hart::SizeError, "All channels must have the same number of frames", {});
            }
        }

        reducerBResults.reserve (numFrames);

        std::vector<FrameIteratorType> channelPositions;
        channelPositions.reserve (static_cast<size_t> (std::distance (begin, end)));

        for (ChannelIteratorType channel = begin; channel != end; ++channel)
            channelPositions.push_back ((*channel).begin());

        std::vector<IteratedValueType<FrameIteratorType>> frameValues;
        frameValues.reserve (channelPositions.size());

        for (size_t frame = 0; frame < numFrames; ++frame)
        {
            frameValues.clear();

            for (auto& position : channelPositions)
            {
                frameValues.push_back (*position);
                ++position;
            }

            reducerBResults.push_back (m_reducerB (frameValues.cbegin(), frameValues.cend()));
        }

        return m_reducerA (reducerBResults.begin(), reducerBResults.end());
    }

private:
    ReducerAType m_reducerA;
    ReducerBType m_reducerB;
    Axis m_axis;
};

/// @brief Reduces each channel or frame, then reduces the intermediate results
/// @details Requires equally sized channels. reducerB reduces each channel with
/// Axis::channels, or each frame across channels with Axis::frames.
/// reducerA then reduces the results from reducerB.
/// Empty ranges are handled by the corresponding reducer.
/// @throws hart::SizeError if channels have different frame counts
template <typename ReducerAType, typename ReducerBType>
Reduce2D<ReducerAType, ReducerBType> reduce2d (ReducerAType reducerA, ReducerBType reducerB, Axis axis = Axis::channels)
{
    return Reduce2D <ReducerAType, ReducerBType> (reducerA, reducerB, axis);
}

/// @brief Returns the number of elements (values) in the range
struct size
{
    template <typename IteratorType>
    size_t operator() (IteratorType begin, IteratorType end) const
    {
        return static_cast<size_t> (std::distance (begin, end));
    }
};

/// @brief Returns the sum of all elements in the range
struct sum
{
    template <typename IteratorType>
    IteratedValueType<IteratorType>
    operator() (IteratorType begin, IteratorType end) const
    {
        if (begin == end)
            HART_THROW_OR_RETURN (hart::SizeError, "The range is empty", hart::nan<IteratedValueType<IteratorType>>());

        AccurateSum<IteratedValueType<IteratorType>> accurateSum;

        for (IteratorType it = begin; it != end; ++it)
            accurateSum += *it;
        
        return accurateSum;
    }
};

/// @}

}  // namespace hart
