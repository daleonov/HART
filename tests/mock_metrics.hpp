#pragma once

#include <functional>
#include <vector>

#include "hart_audio_buffer.hpp"

#include "metrics/hart_metric_query.hpp"
#include "metrics/hart_metrics_common.hpp"  // ChannelSubsets
#include "hart_slice.hpp"
#include "hart_units.hpp"  // Unit

/// @brief Mock test double that is designed to be called once per each channel
/// @private
template <typename ValueType>
hart::MetricQuery<ValueType> mockMetric (const std::vector<ValueType>& values)
{
    const size_t numMockChannels = values.size();

    typename hart::MetricQuery<ValueType>::SingleChannelMetricEvaluator evaluator =
        [&values, numMockChannels]
        (size_t channel, hart::Slice, hart::Unit)
        -> ValueType
    {
        hassert (channel < values.size());
        return values[channel];
    };

    return hart::MetricQuery<ValueType> (
        std::move (evaluator),
        numMockChannels,
        hart::ChannelSubsets::allChannels (numMockChannels)
    );
}

/// @brief A stateful metric returning a regular query for each audio window.
/// @private
template <typename ValueType>
class MockWindowedMetric
{
public:
    explicit MockWindowedMetric (const std::vector<std::vector<ValueType>>& values) :
        m_values (values)
    {
    }

    hart::MetricQuery<ValueType> operator() ()
    {
        const size_t windowIndex = m_windowIndex++;

        typename hart::MetricQuery<ValueType>::SingleChannelMetricEvaluator evaluator =
            [this, windowIndex]
            (size_t channel, hart::Slice, hart::Unit)
            -> ValueType
        {
            hassert (channel < m_values.size());
            hassert (windowIndex < m_values[channel].size());
            return m_values[channel][windowIndex];
        };

        return hart::MetricQuery<ValueType> (
            std::move (evaluator),
            m_values.size(),
            hart::ChannelSubsets::allChannels (m_values.size())
        );
    }

private:
    const std::vector<std::vector<ValueType>>& m_values;
    size_t m_windowIndex = 0;
};

template <typename ValueType>
MockWindowedMetric<ValueType> mockWindowedMetric (const std::vector<std::vector<ValueType>>& values)
{
    return MockWindowedMetric<ValueType> (values);
}
