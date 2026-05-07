#pragma once

#include <functional>
#include <initializer_list>

#include "metrics/hart_metrics_common.hpp"  // ReducerResultType
#include "hart_units.hpp"  // Unit
#include "hart_utils.hpp"  // make_unique()

namespace hart
{

/// @brief Manages the metrics calculations
/// @details This object is meant to be created by metric functions, see @ref Metrics.
/// Usage examples:
/// @code
/// const double resultA = peakLinear (monoBuffer);  // Default unit, implicit cast to double
/// const double resultB = peakLinear (monoBuffer).get();  // Default unit, explicit getter
/// const double resultC = peakLinear (monoBuffer).as (dB);  // Request to calculate in dB
/// const double resultD = peakLinear (monoBuffer).as (linear);  // Request to calculate in linear domain (as voltage, not dB)
/// const double resultE = peakLinear (monoBuffer).as (native);  // Request to calculate in metric's native unit
/// const double resultF = peakLinear (monoBuffer).slice (100, 200).get();  // Peak, observed between 100th (inclusive) and 200th (non-inclusive) frames
/// const double resultG = peakLinear (multiChannelBuffer).as (dB).get (max());  // Get peak of the loudest channel, in dB
/// const double resultH = peakLinear (multiChannelBuffer).as (linear).get (nth (4));  // Get peak of the 4th channel (zero-based), as a linear value
/// const double resultI = peakLinear (multiChannelBuffer).ch ({3, 0, 5}).get (max ());  // Peak, calculated only for channels 3, 0 and 5, max value (requested order of channels will be preserved for order-sensitive reducers)
/// const size_t resultJ = peakLinear (multiChannelBuffer).get (argmax());  // Get the index of the loudest channel
/// const vector<double> resultK = peakLinear (multiChannelBuffer).as (dB).get (collect());  // Get vector of per-channel peak values in dB
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
    using MetricEvaluator = std::function<ValueType (size_t channel, size_t sliceStart, size_t sliceStop, Unit requestedUnit)>;

    /// @brief Create a metric query object
    /// @details This ctor in meant to be invoked by the metric functions,
    /// as they return an object of this type.
    /// @param evaluator A callable object that calculates a specific metric, created by the metric function
    /// @param totalNumChannels Total number of channels in the received AudioBuffer or other container, observed by the metric function
    /// @param totalLength Total length of the container (for one channel) passed to the metric function, observed by the metric function.
    /// For inctance, number of frames in an AudioBuffer, or number of bins in a Spectrum, per one channel.
    MetricQuery (
        MetricEvaluator evaluator,
        size_t totalNumChannels,
        size_t totalLength
        )
    {
        m_query = hart::make_unique<Query>();
        m_query->evaluator = std::move (evaluator);
        m_query->totalNumChannels = totalNumChannels;
        m_query->sliceStart = 0;
        m_query->sliceStop = totalLength;
        m_query->requestedUnit = Unit::native;
    }

    /// @brief Requests the metric to return ints value(s) in a certain unit
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

    /// @brief Requests the metric to be applied to certain channels.
    /// @param channels List of zero-based channel indices to measure. You may
    /// use values from @ref hart::Channel or @ref hart::MidSideChannel where
    /// appropriate. The order of values handed to the reducer matches the order
    /// of channel indices in `channels`, or natural channel order if `channels`
    /// is empty. Passing an empty list to this method will enable all the channels
    MetricQuery ch (std::initializer_list<size_t> channels) const
    {
        MetricQuery copy (*this);
        copy.m_query = hart::make_unique<Query> (*m_query);
        copy.m_query->channels.assign (channels.begin(), channels.end());
        return copy;
    }

    /// @brief Requests to perform a metric on a specific range inside of data
    /// @details The actual meaning of the slice depends on what kind of data
    /// the metric is performed on. For example, for time-domain AudioBuffer
    /// it's the range of frames, for frequency-domain data it's range of bins.
    /// @param sliceStart Start ofn the slice, inclusive
    /// @param sliceStop End of the slice, non-inclusive
    /// @throws hart::SizeError If the the slice is empty
    MetricQuery slice (int sliceStart, int sliceStop) const
    {
        MetricQuery copy (*this);
        copy.m_query = hart::make_unique<Query> (*m_query);
        copy.m_query->sliceStart = sliceStart;
        copy.m_query->sliceStop = sliceStop;
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
        -> ReducerResultType<ReducerType, std::vector<double>::const_iterator>
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
        MetricEvaluator evaluator;
        std::vector<size_t> channels;
        size_t totalNumChannels = 0;
        int64_t sliceStart = 0;
        int64_t sliceStop = 0;
        Unit requestedUnit = Unit::native;
        mutable bool cacheValid = false;
        mutable std::vector<ValueType> cachedValues;
    };

    void ensureCache() const
    {
        if (m_query->cacheValid)
            return;

        if (m_query->sliceStop < m_query->sliceStart)
            HART_THROW_OR_RETURN_VOID (hart::SizeError, "Requester slice's start is greater than its stop");

        m_query->cachedValues.clear();

        for (size_t channel : getChannelIndicesToProcess())
        {
            if (channel >= m_query->totalNumChannels)
                HART_THROW_OR_RETURN_VOID (hart::IndexError, "Requested channel index is out of range");

            m_query->cachedValues.push_back (
                m_query->evaluator (
                    channel,
                    m_query->sliceStart,
                    m_query->sliceStop,
                    m_query->requestedUnit
                )
            );
        }

        m_query->cacheValid = true;
    }

    std::vector<size_t> getChannelIndicesToProcess() const
    {
        if (m_query->channels.size() != 0)
            return m_query->channels;

        std::vector<size_t> indices (m_query->totalNumChannels);

        for (size_t i = 0; i < m_query->totalNumChannels; ++i)
            indices[i] = i;

        return indices;
    }

    std::shared_ptr<Query> m_query;
};

}  // namespace hart
