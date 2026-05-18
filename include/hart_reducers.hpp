#pragma once

#include <algorithm>
#include <cmath>
#include <iterator>
#include <vector>

#include "hart_accurate_sum.hpp"
#include "hart_exceptions.hpp"
#include "hart_utils.hpp"  // nan(), floatsNotEqual(), Interpolation

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
/// @param interpolation Type of interpolation, nearest or linear
struct percentile
{
    percentile (double quantile, Interpolation interpolation = Interpolation::nearest) :
        m_quantile (quantile),
        m_interpolation (interpolation)
    {
    }

    template <typename IteratorType>
    auto operator() (IteratorType begin, IteratorType end) const
        -> IteratedValueType<IteratorType>
    {
        hassert (m_interpolation == Interpolation::nearest || m_interpolation == Interpolation::linear)

        using ValueType = IteratedValueType<IteratorType>;

        if (m_quantile < 0 || m_quantile > 1)
            HART_THROW_OR_RETURN (hart::ValueError, "Quantile value should be in 0..1 range", hart::nan<ValueType>());

        if (begin == end)
            HART_THROW_OR_RETURN (hart::SizeError, "The range is empty", hart::nan<ValueType>());

        const size_t n = static_cast<size_t> (std::distance (begin, end));
        std::vector<ValueType> data (begin, end);

        if (m_interpolation == Interpolation::nearest)
        {
            // Nearest-rank method, get an actual observed value
            const size_t index = static_cast<size_t> (std::ceil (m_quantile * (n - 1)));

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
