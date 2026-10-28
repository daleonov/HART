#pragma once

#include <cmath>
#include <functional>
#include <utility>
#include <vector>

#include "hart_audio_buffer.hpp"
#include "hart_reducers.hpp"

// TODO: Come up with handling occasional NaN returns from the metric

namespace hart
{

template <typename SampleType, typename ValueType = double>
class WindowedSingleBufferMetricQuery
{
public:
    /// @brief Calculates all selected channel or channel-pair values for one window of one buffer.
    using SingleBufferPerWindowMetricEvaluator =
        std::function<std::vector<ValueType> (const AudioBuffer<SampleType>&)>;

    /// @brief Result after applying the inner reducer, then the final reducer.
    template <typename ReducerAType, typename ReducerBType>
    using ResultType =
        ReducerResultType<ReducerAType, typename std::vector<
            ReducerResultType<ReducerBType, typename std::vector<ValueType>::const_iterator>
            >::const_iterator>;

    /// @brief Per-channel value type returned by a per-window callback.
    template <typename SingleBufferPerWindowMetricFunctor>
    using PerWindowMetricValueType = typename decltype (
        std::declval<SingleBufferPerWindowMetricFunctor&>() (
            std::declval<const AudioBuffer<SampleType>&>()))::value_type;

    /// @brief Creates an instance of WindowedSingleBufferMetricQuery
    /// @details You don't have to call this ctor directly - use the `windowed()` factory instead.
    explicit WindowedSingleBufferMetricQuery (const AudioBuffer<SampleType>& audioBuffer) :
        m_audioBuffer (audioBuffer)
    {
    }

    /// @brief Reduce the results of windowed metric
    /// @details Call this at the end of building this chain windowed metric query.
    /// The metric callback set with `metric()` method will be called repeatedly, with
    /// a window from an audio buffer provided in the ctor. The result will be a 2D
    /// vector - outer vector contains per-channel data, inner vectors contain the results
    /// of metric for specific channel for each window. This call is equivalent to calling
    /// `reduce2d()` on such a vector - see its documentation for details.
    /// @return Result of reducing the windowed per-channel data.
    template <typename ReducerAType, typename ReducerBType>
    auto get (ReducerAType reducerA, ReducerBType reducerB, Axis axis = Axis::channels) const
        -> ResultType<ReducerAType, ReducerBType>
    {
        const auto results = calculateWindowedMetricResults();
        return reduce2d (reducerA, reducerB, axis) (results.cbegin(), results.cend());
    }

    /// @brief Sets a metric callback that takes one audio buffer window
    /// @details The audio buffer provided in the windowed metric query ctor will
    /// be cut into smaller windows, obeying windowing settings, and then this
    /// callback will be invoked repeatedly, each time with a new piece of audio
    /// representing each window. Thus, the metric will be calculated for each window
    /// independently.
    /// @param calculateMetricForOneWindow A lambda or callable with a signature:
    /// `std::vector<ValueType> (const AudioBuffer<SampleType>&)`.
    /// The callback may construct spectra or impulse responses internally, but
    /// must return calculated values, e.g. via MetricQuery::collect().
    /// @return A query whose value type is inferred from the callback result.
    template <typename SingleBufferPerWindowMetricFunctor>
    auto metric (SingleBufferPerWindowMetricFunctor calculateMetricForOneWindow) const
        -> WindowedSingleBufferMetricQuery<SampleType, PerWindowMetricValueType<SingleBufferPerWindowMetricFunctor>>
    {
        WindowedSingleBufferMetricQuery<SampleType, PerWindowMetricValueType<SingleBufferPerWindowMetricFunctor>> query (m_audioBuffer);
        query.setMetric (std::move (calculateMetricForOneWindow));

        return query;
    }

private:
    // metric() can change ValueType, so different specializations need access
    // to each other's private setter (e.g. double -> int).
    template <typename, typename> friend class WindowedSingleBufferMetricQuery;

    void setMetric (SingleBufferPerWindowMetricEvaluator evaluator)
    {
        if (evaluator == nullptr)
            HART_THROW_OR_RETURN_VOID (NullPointerError, "The provided functor is null");
    
        m_calculateMetricForOneWindow = std::move (evaluator);
    }

    std::vector<std::vector<ValueType>> calculateWindowedMetricResults() const
    {
        if (! m_calculateMetricForOneWindow)
            HART_THROW_OR_RETURN (StateError, "No window metric provided", {});

        if (! m_audioBuffer.hasSampleRate())
            HART_THROW_OR_RETURN (SampleRateError, "Windowing requires a sample rate", {});

        const double sampleRate = m_audioBuffer.getSampleRateHz();

        if (! std::isfinite (sampleRate) || sampleRate <= 0.0)
            HART_THROW_OR_RETURN (SampleRateError, "Windowing requires a positive finite sample rate", {});

        const size_t windowFrames = roundToSizeT (sampleRate * 0.1);

        if (windowFrames == 0 || windowFrames > m_audioBuffer.getNumFrames())
            HART_THROW_OR_RETURN (SizeError, "No complete window fits in the buffer", {});

        const size_t numWindows = m_audioBuffer.getNumFrames() / windowFrames;
        AudioBuffer<SampleType> frame (m_audioBuffer.getNumChannels(), windowFrames, sampleRate);
        std::vector<std::vector<ValueType>> results;

        // Copy the callback so mutable per-analysis state starts afresh on get().
        auto evaluator = m_calculateMetricForOneWindow;

        for (size_t window = 0; window < numWindows; ++window)
        {
            for (size_t channel = 0; channel < m_audioBuffer.getNumChannels(); ++channel)
                frame.copyFrom (channel, 0, m_audioBuffer, channel, window * windowFrames, windowFrames);

            const auto values = evaluator (frame);

            if (window == 0)
                results.resize (values.size());

            if (values.size() != results.size())
                HART_THROW_OR_RETURN (SizeError, "Window metric result sizes differ", {});

            for (size_t channel = 0; channel < values.size(); ++channel)
                results[channel].push_back (values[channel]);
        }

        return results;
    }

    const AudioBuffer<SampleType>& m_audioBuffer;
    SingleBufferPerWindowMetricEvaluator m_calculateMetricForOneWindow;
};

/// @brief Creates a windowed metric query instance
/// @details Use it to apply existing metrics in a windowed manner.
/// A piece of audio provided in @p audioBuffer will be cut into windows,
/// obeying the optional window settings, then the metric will be calculated
/// independently for each window, then the result can be reduced to a scalar
/// via `WindowedSingleBufferMetricQuery::get()`. See `WindowedSingleBufferMetricQuery` for more details.
template <typename SampleType>
WindowedSingleBufferMetricQuery<SampleType> windowed (const AudioBuffer<SampleType>& audioBuffer)
{
    return WindowedSingleBufferMetricQuery<SampleType> (audioBuffer);
}

}  // namespace hart
