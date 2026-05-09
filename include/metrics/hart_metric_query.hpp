#pragma once

#include <functional>
#include <initializer_list>
#include <utility>  // pair
#include <vector>

#include "hart_exceptions.hpp"
#include "metrics/hart_metrics_common.hpp"  // ReducerResultType
#include "hart_reducers.hpp"  // first
#include "hart_slice.hpp"
#include "hart_units.hpp"  // Unit
#include "hart_utils.hpp"  // make_unique()

namespace hart
{

/// @brief Manages the metrics calculations
/// @details This object is meant to be created by metric functions, see @ref Metrics.
/// Usage examples:
/// @code
/// const double resultA = samplePeak (monoBuffer);  // Default unit, implicit cast to double
/// const double resultB = samplePeak (monoBuffer).get();  // Default unit, explicit getter
/// const double resultC = samplePeak (monoBuffer).as (dB);  // Request to calculate in dB
/// const double resultD = samplePeak (monoBuffer).as (linear);  // Request to calculate in linear domain (as voltage, not dB)
/// const double resultE = samplePeak (monoBuffer).as (native);  // Request to calculate in metric's native unit
/// const double resultF = samplePeak (monoBuffer).slice (100, 200).get();  // Peak, observed between 100th (inclusive) and 200th (non-inclusive) frames
/// const double resultG = samplePeak (multiChannelBuffer).as (dB).get (max());  // Get peak of the loudest channel, in dB
/// const double resultH = samplePeak (multiChannelBuffer).as (linear).get (nth (4));  // Get peak of the 4th channel (zero-based), as a linear value
/// const double resultI = samplePeak (multiChannelBuffer).ch ({3, 0, 5}).get (max ());  // Peak, calculated only for channels 3, 0 and 5, max value (requested order of channels will be preserved for order-sensitive reducers)
/// const size_t resultJ = samplePeak (multiChannelBuffer).get (argmax());  // Get the index of the loudest channel
/// const vector<double> resultK = samplePeak (multiChannelBuffer).as (dB).get (collect());  // Get vector of per-channel peak values in dB
/// @endcode
/// If you create your own metric functions, it's strongly encouraged for them to return this class instance,
/// rather that just a POD value, as this class handles multi-channel setups, slices etc.
/// @ingroup Metrics
template <typename ValueType>
class MetricQuery
{
public:
    /// @brief A lambda function (or a callable object) that calculates a specific metric for a given channel
    /// @details Arguments:
    ///  - `channel` - number of channel to calculate metric (for metrics that operate per channel), see `ch()`
    ///  - `sliceStart`, `sliceStop` - the range of data to calculate the metric on, see `slice()`
    ///  - `requestedUnit` - the unit that the metric should be calculated in, see `as()`
    using SingleChannelMetricEvaluator = std::function<ValueType (size_t channel, Slice slice, Unit requestedUnit)>;

    /// @brief A lambda function (or a callable object) that calculates a specific metric for a given pair of channels
    /// @details Arguments:
    ///  - `channelA` - number of left-hand-side channel to calculate metric (for metrics that operate per channel), see `ch()`
    ///  - `channelB` - number of right-hand-side channel to calculate metric (for metrics that operate per channel), see `ch()`
    ///  - `sliceStart`, `sliceStop` - the range of data to calculate the metric on, see `slice()`
    ///  - `requestedUnit` - the unit that the metric should be calculated in, see `as()`
    using ChannelPairMetricEvaluator = std::function<ValueType (size_t channelA, size_t channelB, Slice slice, Unit requestedUnit)>;

    /// @brief Create a metric query object for a metric that operates on one channel at a time
    /// @details This ctor in meant to be invoked by the metric functions,
    /// as they return an object of this type.
    /// @param evaluator A callable object that calculates a specific metric, created by the metric function
    /// @param totalNumChannels Total number of channels in the received AudioBuffer or other container, observed by the metric function
    /// @param totalLength Total length of the container (for one channel) passed to the metric function, observed by the metric function.
    /// For instance, number of frames in an AudioBuffer, or number of bins in a Spectrum, per one channel.
    /// @param defaultChannelsToProcess Subset of channels to process by default, if `ch()` wasn't called.
    MetricQuery (
        SingleChannelMetricEvaluator evaluator,
        size_t totalNumChannels,
        size_t totalLength,
        std::vector<size_t>&& defaultChannelsToProcess
        )
    {
        m_query = hart::make_unique<Query>();
        m_query->singleChannelMetricEvaluator = std::move (evaluator);
        m_query->totalNumChannelsA = totalNumChannels;
        m_query->totalNumChannelsB = 0;
        m_query->slice = Slice::whole();
        m_query->requestedUnit = Unit::native;
        m_query->channels = std::move (defaultChannelsToProcess);
    }

    /// @brief Create a metric query object for a metric that operates on pair of channels at a time
    /// @details This ctor in meant to be invoked by the metric functions,
    /// as they return an object of this type.
    /// @param evaluator A callable object that calculates a specific metric, created by the metric function
    /// @param totalNumChannelsA Total number of channels in the received left-hand-side AudioBuffer or other container, observed by the metric function
    /// @param totalNumChannelsB Total number of channels in the received right-hand-side AudioBuffer or other container, observed by the metric function.
    /// If metric requires pairs of channels, but operates on a single container, it's expected to be equal to totalNumChannelsA.
    /// @param totalLength Total length of the container (for one channel) passed to the metric function, observed by the metric function.
    /// For instance, number of frames in an AudioBuffer, or number of bins in a Spectrum, per one channel.
    /// Left-hand-side and right-hand-side containers are typically expected to be equal in size (per each channel).
    /// @param defaultChannelPairsToProcess Subset of channel pairs to process by default, if `ch()` wasn't called.
    MetricQuery (
        ChannelPairMetricEvaluator evaluator,
        size_t totalNumChannelsA,
        size_t totalNumChannelsB,
        size_t totalLength,
        std::vector<std::pair<size_t, size_t>>&& defaultChannelPairsToProcess
        )
    {
        m_query = hart::make_unique<Query>();
        m_query->channelPairMetricEvaluator = std::move (evaluator);
        m_query->totalNumChannelsA = totalNumChannelsA;
        m_query->totalNumChannelsB = totalNumChannelsB;
        m_query->slice = Slice::whole();
        m_query->requestedUnit = Unit::native;
        m_query->channelPairs = std::move (defaultChannelPairsToProcess);
    }

    /// @brief Requests the metric to return its value(s) in a certain unit
    /// @param requestedUnit A desired unit that the metric should return.
    /// Refer to the documentation of a specific metric for supported units.
    /// If unsupported unit is requested, the metric is expected to throw a
    /// `hart::UnitError` exception. 
    MetricQuery as (Unit requestedUnit) const
    {
        MetricQuery copy (*this);
        copy.m_query = hart::make_unique<Query> (*m_query);
        copy.m_query->requestedUnit = requestedUnit;
        return copy;
    }

    /// @brief Requests the metric to be applied to certain channel.
    /// @details If this method is not called, the channel subset will be
    /// defined by a specific metric.
    /// @param channel Zero-based channel index to measure. You may
    /// use values from @ref hart::Channel or @ref hart::MidSideChannel
    /// where appropriate.
    MetricQuery ch (size_t channel) const
    {
        MetricQuery copy (*this);
        copy.m_query = hart::make_unique<Query> (*m_query);

        if (getEvaluatorType() == EvaluatorType::singleChannels)
        {
            copy.m_query->channels.clear();
            copy.m_query->channels.push_back (channel);
        }
        else
        {
            copy.m_query->channelPairs.clear();
            copy.m_query->channelPairs.push_back (std::make_pair (channel, channel));
        }

        return copy;
    }

    /// @brief Requests the metric to be applied to certain channels.
    /// @details If this method is not called, the channel subset will be
    /// defined by a specific metric.
    /// @param channels List of zero-based channel indices to measure. You may
    /// use values from @ref hart::Channel or @ref hart::MidSideChannel where
    /// appropriate. The order of values handed to the reducer matches the order
    /// of channel indices in `channels`.
    MetricQuery ch (std::initializer_list<size_t> channels) const
    {
        MetricQuery copy (*this);
        copy.m_query = hart::make_unique<Query> (*m_query);

        if (getEvaluatorType() == EvaluatorType::singleChannels)
        {
            copy.m_query->channels.clear();
            copy.m_query->channels.reserve (channels.size());
            copy.m_query->channels.assign (channels.begin(), channels.end());
        }
        else
        {
            copy.m_query->channelPairs.clear();
            copy.m_query->channelPairs.reserve (channels.size());

            for (const size_t channel : channels)
                copy.m_query->channelPairs.emplace_back (channel, channel);
        }

        return copy;
    }

    /// @brief Requests the metric to be applied to certain channel pairs.
    /// @details If this method is not called, the channel pair subset will
    /// be defined by a specific metric.
    /// @param channels List of pairs of zero-based channel indices to measure.
    /// You may use values from @ref hart::Channel or @ref hart::MidSideChannel
    /// wherever appropriate. The order of values handed to the reducer matches
    /// the order of channel indices in `channels`.
    MetricQuery ch (std::initializer_list<std::pair<size_t, size_t>> channels) const
    {
        MetricQuery copy (*this);
        copy.m_query = hart::make_unique<Query> (*m_query);

        if (getEvaluatorType() == EvaluatorType::singleChannels)
            HART_THROW_OR_RETURN (hart::UnsupportedError, "The metric does not support channel pairs", copy);

        copy.m_query->channelPairs.clear();
        copy.m_query->channels.reserve (channels.size());
        copy.m_query->channelPairs.assign (channels.begin(), channels.end());
        return copy;
    }

    /// @brief Requests to perform a metric on a specific range inside of data
    /// @param slice Slice representing a range of data, see `hart::Slice`
    /// @throws hart::SizeError If the the slice is empty
    MetricQuery at (Slice slice) const
    {
        MetricQuery copy (*this);
        copy.m_query = hart::make_unique<Query> (*m_query);

        if (slice.isEmpty())
            HART_THROW_OR_RETURN (hart::SizeError, "Requested slice is empty", copy);

        copy.m_query->slice = slice;
        return copy;
    }

    /// @brief Query a value of a calculated metric using a reducer
    /// @details Typically, metrics are calculated per channel, which result in a
    /// vector of per-channel values. And in most cases, you just want one scalar,
    /// like a max, min, mean etc. You can choose how to reduce a vector to one
    /// scalar, by providing a reducer. There's a good chance one the built-in
    /// reducers will do what you're looking for, see @ref Reducers. Reducer can
    /// also be a lambda or a callable object. Reducers usually return just one
    /// scalar value, but not always - for example, hart::collect() will just
    /// forward the per-channel vector of values.
    /// @param reducer Callable reducer that accepts two iterators over per-channel
    /// metric values
    template <typename ReducerType>
    auto get (ReducerType reducer) const
        -> ReducerResultType<ReducerType, typename std::vector<ValueType>::const_iterator>
    {
        ensureCache();
        return reducer (m_query->cachedValues.begin(), m_query->cachedValues.end());
    }

    /// @brief Query a value of a calculated metric
    /// @details This overload is useful for mono signals, or metrics that calculate
    /// a scalar value, rather than calculating a per-channel vector of values. For
    /// metrics that are calculated per channel, use an overload of this method that
    /// takes a reducer callable.
    ValueType get() const
    {
        return get (first());
    }

    /// @brief Query a value of a calculated metric
    /// @details This cast is useful for mono signals, or metrics that calculate
    /// a scalar value, rather than calculating a per-channel vector of values. For
    /// metrics that are calculated per channel, use an overload of this method that
    /// takes a reducer callable.
    operator ValueType() const
    {
        return get (first());
    }

private:
    struct Query
    {
        SingleChannelMetricEvaluator singleChannelMetricEvaluator = nullptr;
        ChannelPairMetricEvaluator channelPairMetricEvaluator = nullptr;
        std::vector<size_t> channels;
        std::vector<std::pair<size_t, size_t>> channelPairs;
        size_t totalNumChannelsA = 0;
        size_t totalNumChannelsB = 0;
        Slice slice {Slice::whole()};
        Unit requestedUnit = Unit::native;
        mutable bool cacheValid = false;
        mutable std::vector<ValueType> cachedValues;
    };

    enum class EvaluatorType
    {
        singleChannels,
        channelPairs,
        none
    };

    EvaluatorType getEvaluatorType() const
    {
        hassert (m_query != nullptr);

        if (m_query->singleChannelMetricEvaluator != nullptr)
        {
            hassert (m_query->channelPairMetricEvaluator == nullptr);
            return EvaluatorType::singleChannels;
        }

        if (m_query->channelPairMetricEvaluator != nullptr)
        {
            hassert (m_query->singleChannelMetricEvaluator == nullptr);
            return EvaluatorType::channelPairs;
        }

        hassertfalse;  // Unexpected state - both evaluators are empty!
        return EvaluatorType::none;
    }

    void ensureCache() const
    {
        if (m_query->cacheValid)
            return;

        m_query->cachedValues.clear();

        if (getEvaluatorType() == EvaluatorType::singleChannels)
            buildCacheForSingleChannelsEvaluator();
        else
            buildCacheForChannelPairsEvaluator();

        m_query->cacheValid = true;
    }

    void buildCacheForSingleChannelsEvaluator() const
    {
        for (size_t channel : m_query->channels)
        {
            if (channel >= m_query->totalNumChannelsA)
                HART_THROW_OR_RETURN_VOID (hart::IndexError, "Requested channel index is out of range");

            m_query->cachedValues.push_back (
                m_query->singleChannelMetricEvaluator (
                    channel,
                    m_query->slice,
                    m_query->requestedUnit
                )
            );
        }
    }

    void buildCacheForChannelPairsEvaluator() const
    {
        for (const std::pair<size_t, size_t>& channelPair : m_query->channelPairs)
        {
            if (channelPair.first >= m_query->totalNumChannelsA || channelPair.second >= m_query->totalNumChannelsB)
                HART_THROW_OR_RETURN_VOID (hart::IndexError, "Requested channel index is out of range");

            m_query->cachedValues.push_back (
                m_query->channelPairMetricEvaluator (
                    channelPair.first,
                    channelPair.second,
                    m_query->slice,
                    m_query->requestedUnit
                )
            );
        }
    }

    std::shared_ptr<Query> m_query;
};

}  // namespace hart
