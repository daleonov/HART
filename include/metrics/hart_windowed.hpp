#pragma once

#include <cmath>
#include <functional>
#include <utility>
#include <vector>

#include "hart_accurate_sum.hpp"
#include "hart_audio_buffer.hpp"
#include "hart_reducers.hpp"

// TODO: Come up with handling occasional NaN returns from the metric

namespace hart
{

namespace internal
{

/// @private Copies source samples into reusable storage for one analysis window.
template <typename SampleType>
void copyWindowedAudioFrame (const AudioBuffer<SampleType>& source, AudioBuffer<SampleType>& frame, size_t startFrame)
{
    for (size_t channel = 0; channel < source.getNumChannels(); ++channel)
        frame.copyFrom (channel, 0, source, channel, startFrame, frame.getNumFrames());
}

/// @private Shared execution and reduction for HART's concrete windowed queries.
/// This CRTP base is an implementation detail, not a user extension point.
template <typename SampleType, typename ValueType, typename DerivedQueryType>
class WindowedMetricQueryBase
{
public:
    /// @brief Result after applying the inner reducer, then the final reducer.
    template <typename ReducerAType, typename ReducerBType>
    using ResultType =
        ReducerResultType<ReducerAType, typename std::vector<
            ReducerResultType<ReducerBType, typename std::vector<ValueType>::const_iterator>
            >::const_iterator>;

    /// @brief Requests a window size in seconds
    /// @details The window size defaults to 100 ms. Only complete windows are processed.
    /// Window boundaries are converted from time to frames independently, so window
    /// lengths may differ by one frame. With the default step, this does not leave
    /// gaps or use a frame twice.
    /// @return A copy of this query with the requested window size.
    DerivedQueryType size (double windowSizeSeconds) const
    {
        const auto& query = static_cast<const DerivedQueryType&> (*this);

        if (! std::isfinite (windowSizeSeconds) || windowSizeSeconds <= 0.0)
            HART_THROW_OR_RETURN (ValueError, "Window size must be positive and finite", query);

        DerivedQueryType copy (query);
        copy.m_windowSizeSeconds = windowSizeSeconds;

        return copy;
    }

    /// @brief Requests a window step in seconds
    /// @details The step defaults to the window size.
    /// Smaller steps overlap windows, larger steps leave gaps.
    /// @return A chainable copy of this query with the requested window step.
    DerivedQueryType step (double windowStepSeconds) const
    {
        const auto& query = static_cast<const DerivedQueryType&> (*this);

        // Internally, NaN, which technically fails this check, is
        // used to indicate default step, but we won't allow to
        // explicitly set it to NaN via this method.
        if (! std::isfinite (windowStepSeconds))
            HART_THROW_OR_RETURN (ValueError, "Window step must be finite", query);

        if (hart::floatIsZero (windowStepSeconds))
            HART_THROW_OR_RETURN (ValueError, "Window step cannot be zero", query);

        if (windowStepSeconds < 0.0)
            HART_THROW_OR_RETURN (ValueError, "Window step cannot be negative", query);

        DerivedQueryType copy (query);

        copy.m_windowStepSeconds = windowStepSeconds;

        return copy;
    }

    /// @brief Reduce the results of windowed metric
    /// @details Call this at the end of building this chain windowed metric query.
    /// The metric callback set with `metric()` method will be called repeatedly, with
    /// a window from each audio buffer provided in the ctor. The result will be a 2D
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

protected:
    double m_windowSizeSeconds = 0.1;
    double m_windowStepSeconds = hart::nan<double>();

    WindowedMetricQueryBase() = default;
    explicit WindowedMetricQueryBase (double windowSizeSeconds, double windowStepSeconds) :
        m_windowSizeSeconds (windowSizeSeconds),
        m_windowStepSeconds (windowStepSeconds)
    {}
    ~WindowedMetricQueryBase() = default;

    WindowedMetricQueryBase (const WindowedMetricQueryBase&) = default;
    WindowedMetricQueryBase (WindowedMetricQueryBase&&) = default;
    WindowedMetricQueryBase& operator= (const WindowedMetricQueryBase&) = default;
    WindowedMetricQueryBase& operator= (WindowedMetricQueryBase&&) = default;

    static bool isWindowedAudioSourceValid (const AudioBuffer<SampleType>& source)
    {
        if (! source.hasSampleRate())
            HART_THROW_OR_RETURN (SampleRateError, "Windowing requires a sample rate", false);

        const double sampleRate = source.getSampleRateHz();

        if (! std::isfinite (sampleRate) || sampleRate <= 0.0)
            HART_THROW_OR_RETURN (SampleRateError, "Windowing requires a positive finite sample rate", false);

        return true;
    }

private:
    std::vector<std::vector<ValueType>> calculateWindowedMetricResults() const
    {
        const auto& query = static_cast<const DerivedQueryType&> (*this);
        if (! query.hasMetric())
            HART_THROW_OR_RETURN (StateError, "No window metric provided", {});

        if (! query.validateInputBuffers())
            return {};

        const auto& source = query.primaryBuffer();
        const double requestedWindowFrames = source.getSampleRateHz() * m_windowSizeSeconds;

        if (! std::isfinite (requestedWindowFrames) || requestedWindowFrames > static_cast<double> (source.getNumFrames()) + 0.5)
            HART_THROW_OR_RETURN (SizeError, "No complete window fits in the buffer", {});

        const size_t windowFrames = roundToSizeT (requestedWindowFrames);

        if (windowFrames == 0 || windowFrames > source.getNumFrames())
            HART_THROW_OR_RETURN (SizeError, "No complete window fits in the buffer", {});

        // Scratch buffers and a copy of the callback live only for this get().
        auto execution = query.createWindowExecution (windowFrames);
        std::vector<std::vector<ValueType>> results;
        const double windowStepSeconds = std::isnan (m_windowStepSeconds) ? m_windowSizeSeconds : m_windowStepSeconds;
        AccurateSum<double> windowStartSeconds;
        AccurateSum<double> windowStopSeconds (m_windowSizeSeconds);
        bool hasWindow = false;

        while (true)
        {
            // Accumulate start and stop times, not rounded frame counts. The
            // default step keeps adjacent boundaries equal; custom steps allow
            // intentional overlaps or gaps without accumulating rounding drift.
            const double stopFramePosition = windowStopSeconds.getValue() * source.getSampleRateHz();

            if (! std::isfinite (stopFramePosition) || stopFramePosition > static_cast<double> (source.getNumFrames()) + 0.5)
                break;

            const size_t startFrame = roundToSizeT (windowStartSeconds.getValue() * source.getSampleRateHz());
            const size_t stopFrame = roundToSizeT (stopFramePosition);

            if (stopFrame > source.getNumFrames())
                break;  // Skip a partial window.

            if (stopFrame <= startFrame)
                HART_THROW_OR_RETURN (SizeError, "Window size must produce non-empty windows", {});

            const auto values = execution.calculateWindow (startFrame, stopFrame);

            if (! hasWindow)
            {
                results.resize (values.size());
                hasWindow = true;
            }

            if (values.size() != results.size())
                HART_THROW_OR_RETURN (SizeError, "Window metric result sizes differ", {});

            for (size_t channel = 0; channel < values.size(); ++channel)
                results[channel].push_back (values[channel]);

            windowStartSeconds += windowStepSeconds;
            windowStopSeconds += windowStepSeconds;
        }

        if (! hasWindow)
            HART_THROW_OR_RETURN (SizeError, "No complete window fits in the buffer", {});

        return results;
    }
};

}  // namespace internal

template <typename SampleType, typename ValueType = double>
class WindowedSingleBufferMetricQuery :
    public internal::WindowedMetricQueryBase<SampleType, ValueType, WindowedSingleBufferMetricQuery<SampleType, ValueType>>
{
    using BaseType = internal::WindowedMetricQueryBase<SampleType, ValueType, WindowedSingleBufferMetricQuery<SampleType, ValueType>>;

public:
    /// @brief Calculates all selected channel or channel-pair values for one window of one buffer.
    using SingleBufferPerWindowMetricEvaluator =
        std::function<std::vector<ValueType> (const AudioBuffer<SampleType>&)>;

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
        WindowedSingleBufferMetricQuery<SampleType, PerWindowMetricValueType<SingleBufferPerWindowMetricFunctor>> query (m_audioBuffer, this->m_windowSizeSeconds, this->m_windowStepSeconds);
        query.setMetric (std::move (calculateMetricForOneWindow));
        return query;
    }

private:
    // metric() can change ValueType, so specializations need each other's setter.
    template <typename, typename> friend class WindowedSingleBufferMetricQuery;
    friend BaseType;

    WindowedSingleBufferMetricQuery (const AudioBuffer<SampleType>& audioBuffer, double windowSizeSeconds, double windowStepSeconds) :
        BaseType (windowSizeSeconds, windowStepSeconds),
        m_audioBuffer (audioBuffer)
    {
    }

    struct WindowExecution
    {
        WindowExecution (const AudioBuffer<SampleType>& sourceBuffer, size_t windowFrames, SingleBufferPerWindowMetricEvaluator evaluator) :
            source (sourceBuffer),
            frame (source.getNumChannels(), windowFrames, source.getSampleRateHz()),
            calculateMetric (std::move (evaluator))
        {
        }

        std::vector<ValueType> calculateWindow (size_t startFrame, size_t stopFrame)
        {
            frame.setNumFrames (stopFrame - startFrame);
            internal::copyWindowedAudioFrame (source, frame, startFrame);
            return calculateMetric (frame);
        }

        const AudioBuffer<SampleType>& source;
        AudioBuffer<SampleType> frame;
        SingleBufferPerWindowMetricEvaluator calculateMetric;
    };

    bool hasMetric() const { return m_calculateMetricForOneWindow != nullptr; }
    const AudioBuffer<SampleType>& primaryBuffer() const { return m_audioBuffer; }
    bool validateInputBuffers() const { return BaseType::isWindowedAudioSourceValid (m_audioBuffer); }

    WindowExecution createWindowExecution (size_t windowFrames) const
    {
        return WindowExecution (m_audioBuffer, windowFrames, m_calculateMetricForOneWindow);
    }

    void setMetric (SingleBufferPerWindowMetricEvaluator evaluator)
    {
        if (evaluator == nullptr)
            HART_THROW_OR_RETURN_VOID (NullPointerError, "The provided functor is null");
        m_calculateMetricForOneWindow = std::move (evaluator);
    }

    const AudioBuffer<SampleType>& m_audioBuffer;
    SingleBufferPerWindowMetricEvaluator m_calculateMetricForOneWindow;
};

template <typename SampleType, typename ValueType = double>
class WindowedBufferPairMetricQuery :
    public internal::WindowedMetricQueryBase<SampleType, ValueType, WindowedBufferPairMetricQuery<SampleType, ValueType>>
{
    using BaseType = internal::WindowedMetricQueryBase<SampleType, ValueType, WindowedBufferPairMetricQuery<SampleType, ValueType>>;

public:
    /// @brief Calculates selected channel or channel-pair values for synchronized windows from two buffers.
    using BufferPairPerWindowMetricEvaluator =
        std::function<std::vector<ValueType> (const AudioBuffer<SampleType>&, const AudioBuffer<SampleType>&)>;

    /// @brief Per-channel value type returned by a per-window callback.
    template <typename BufferPairPerWindowMetricFunctor>
    using PerWindowMetricValueType = typename decltype (
        std::declval<BufferPairPerWindowMetricFunctor&>() (
            std::declval<const AudioBuffer<SampleType>&>(), std::declval<const AudioBuffer<SampleType>&>()))::value_type;

    /// @brief Creates an instance of WindowedBufferPairMetricQuery
    /// @details You don't have to call this ctor directly - use the `windowed()` factory instead.
    WindowedBufferPairMetricQuery (const AudioBuffer<SampleType>& audioBufferA, const AudioBuffer<SampleType>& audioBufferB) :
        m_audioBufferA (audioBufferA),
        m_audioBufferB (audioBufferB)
    {
    }

    /// @brief Sets a metric callback that takes two synchronized audio buffer windows
    /// @details The audio buffers provided in the windowed metric query ctor will
    /// be cut into smaller windows, obeying windowing settings, and then this
    /// callback will be invoked repeatedly, each time with matching pieces of audio
    /// representing each window. Thus, the metric will be calculated for each window
    /// independently.
    /// @param calculateMetricForOneWindow A lambda or callable with a signature:
    /// `std::vector<ValueType> (const AudioBuffer<SampleType>&, const AudioBuffer<SampleType>&)`.
    /// The callback may construct spectra or impulse responses internally, but
    /// must return calculated values, e.g. via MetricQuery::collect().
    /// @return A query whose value type is inferred from the callback result.
    template <typename BufferPairPerWindowMetricFunctor>
    auto metric (BufferPairPerWindowMetricFunctor calculateMetricForOneWindow) const
        -> WindowedBufferPairMetricQuery<SampleType, PerWindowMetricValueType<BufferPairPerWindowMetricFunctor>>
    {
        WindowedBufferPairMetricQuery<SampleType, PerWindowMetricValueType<BufferPairPerWindowMetricFunctor>> query (m_audioBufferA, m_audioBufferB, this->m_windowSizeSeconds, this->m_windowStepSeconds);
        query.setMetric (std::move (calculateMetricForOneWindow));
        return query;
    }

private:
    template <typename, typename> friend class WindowedBufferPairMetricQuery;
    friend BaseType;

    WindowedBufferPairMetricQuery (
        const AudioBuffer<SampleType>& audioBufferA,
        const AudioBuffer<SampleType>& audioBufferB,
        double windowSizeSeconds,
        double windowStepSeconds
        ) :
        BaseType (windowSizeSeconds, windowStepSeconds),
        m_audioBufferA (audioBufferA),
        m_audioBufferB (audioBufferB)
    {
    }

    struct WindowExecution
    {
        WindowExecution (
            const AudioBuffer<SampleType>& bufferA,
            const AudioBuffer<SampleType>& bufferB,
            size_t windowFrames, BufferPairPerWindowMetricEvaluator evaluator
            ) :
            sourceA (bufferA),
            sourceB (bufferB),
            frameA (sourceA.getNumChannels(), windowFrames, sourceA.getSampleRateHz()),
            frameB (sourceB.getNumChannels(), windowFrames, sourceB.getSampleRateHz()),
            calculateMetric (std::move (evaluator))
        {
        }

        std::vector<ValueType> calculateWindow (size_t startFrame, size_t stopFrame)
        {
            frameA.setNumFrames (stopFrame - startFrame);
            frameB.setNumFrames (stopFrame - startFrame);
            internal::copyWindowedAudioFrame (sourceA, frameA, startFrame);
            internal::copyWindowedAudioFrame (sourceB, frameB, startFrame);
            return calculateMetric (frameA, frameB);
        }

        const AudioBuffer<SampleType>& sourceA;
        const AudioBuffer<SampleType>& sourceB;
        AudioBuffer<SampleType> frameA;
        AudioBuffer<SampleType> frameB;
        BufferPairPerWindowMetricEvaluator calculateMetric;
    };

    bool hasMetric() const { return m_calculateMetricForOneWindow != nullptr; }
    const AudioBuffer<SampleType>& primaryBuffer() const { return m_audioBufferA; }

    bool validateInputBuffers() const
    {
        if (! BaseType::isWindowedAudioSourceValid (m_audioBufferA) || ! BaseType::isWindowedAudioSourceValid (m_audioBufferB))
            return false;

        if (m_audioBufferA.getSampleRateHz() != m_audioBufferB.getSampleRateHz())
            HART_THROW_OR_RETURN (SampleRateError, "Windowed buffers must have equal sample rates", false);

        if (m_audioBufferA.getNumFrames() != m_audioBufferB.getNumFrames())
            HART_THROW_OR_RETURN (SizeError, "Windowed buffers must have equal lengths", false);

        return true;
    }

    WindowExecution createWindowExecution (size_t windowFrames) const
    {
        return WindowExecution (m_audioBufferA, m_audioBufferB, windowFrames, m_calculateMetricForOneWindow);
    }

    void setMetric (BufferPairPerWindowMetricEvaluator evaluator)
    {
        if (evaluator == nullptr)
            HART_THROW_OR_RETURN_VOID (NullPointerError, "The provided functor is null");

        m_calculateMetricForOneWindow = std::move (evaluator);
    }

    const AudioBuffer<SampleType>& m_audioBufferA;
    const AudioBuffer<SampleType>& m_audioBufferB;
    BufferPairPerWindowMetricEvaluator m_calculateMetricForOneWindow;
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

/// @brief Creates a windowed metric query instance for a pair of buffers
/// @details Use it to apply existing metrics in a windowed manner.
/// Audio provided in @p audioBufferA and @p audioBufferB will be cut into matching windows,
/// obeying the optional window settings, then the metric will be calculated
/// independently for each window, then the result can be reduced to a scalar
/// via `WindowedBufferPairMetricQuery::get()`. See `WindowedBufferPairMetricQuery` for more details.
/// Both buffers must have equal positive finite sample rates and equal frame counts.
/// Channel counts may differ. Both buffers must remain alive until get() completes.
template <typename SampleType>
WindowedBufferPairMetricQuery<SampleType> windowed (
    const AudioBuffer<SampleType>& audioBufferA,
    const AudioBuffer<SampleType>& audioBufferB)
{
    return WindowedBufferPairMetricQuery<SampleType> (audioBufferA, audioBufferB);
}

}  // namespace hart
