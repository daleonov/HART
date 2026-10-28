#pragma once

#include <vector>

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
