#pragma once

#include <vector>
#include <chrono>

#include "metrics/hart_metrics_common.hpp"  // ReducerResultType

namespace hart
{

/// @brief Measures execution time of a code block over multiple iterations.
///
/// This macro runs the provided code block @p numRuns times and records the duration
/// of each iteration into a local `hart::TimeIt` instance.
///
/// The recorder object is declared with the name provided in @p timeItInstanceName and can
/// be queried after the block using reducers such as `hart::mean()`, `hart::max()`,
/// `hart::percentile()` etc (see @ref Reducers).
///
/// To query the result, call `hart::TimeIt::result()` on the created `TimeIt` instance.
///
/// @par Example
/// @code
/// HART_TIME_IT (myTimeIt, 100)
/// {
///     doSomething();
/// };
///
/// const double t = myTimeIt.result (hart::mean());
/// @endcode
///
/// @param timeItInstanceName Name of the `hart::TimeIt` instance to create.
/// Pick any valid name, and the object with such name will be created by the macro.
/// @param numRuns Number of iterations to run the block.
///
/// @warning This macro expands to multiple statements and behaves like a control
/// statement. It must always be followed by a block. If nested, it must always
/// be surrounded by braces. In particular, do NOT use it like this:
/// @code
/// if (someCondition)
///     HART_TIME_IT (myTimeIt, 100) { doSomething(); }  // Incorrect! It's not a single statement.
/// else
///     doSomethingElse();
/// @endcode
/// Instead, always use braces:
/// @code
/// if (cond)
/// {
///     HART_TIME_IT (myTimeIt, 100)
///     {
///         doSomething();
///     };
/// }
/// else
/// {
///     doSomethingElse();
/// }
/// @endcode
///
/// @note If the code inside the block throws an exception, the current iteration
/// will still be recorded (via RAII), but may include stack unwinding time.
/// Subsequent iterations will not run.
/// @ingroup Utilities
#define HART_TIME_IT(timeItInstanceName, numRuns) \
    ::hart::TimeIt timeItInstanceName (numRuns); \
    for (size_t HART_timeItCurrentRun = 0; HART_timeItCurrentRun < (numRuns); ++HART_timeItCurrentRun) \
        for (bool HART_timeItRunOnce = true; HART_timeItRunOnce; HART_timeItRunOnce = false) \
            for (::hart::TimeItSample HART_sample (timeItInstanceName); HART_timeItRunOnce; HART_timeItRunOnce = false)

/// @brief Helper class for measuring duration of some block of code over multiple runs
/// @details Don't instantiate this class directly, it's intended to be created via `HART_TIME_IT()` macro
/// @ingroup Utilities
class TimeIt
{
public:
    /// @brief Creates a new TimeIt instance
    /// @param numRuns Number of times to run the measured block of code
    TimeIt (size_t numRuns)
    {
        m_observedDurationsSeconds.reserve (numRuns);
    }

    /// @brief Record an observed duration
    /// @param duration Observed code block duration in seconds
    /// @private
    void add (double durationSeconds)
    {
        m_observedDurationsSeconds.push_back (durationSeconds);
    }

    /// @brief Get the result of the timing measurement
    /// @details Internally, it stores a list of durations for each run. You're expected to choose
    /// what to do with this value by providing a reducer instance like `hart::mean()`, `hart::max()`.
    /// If you want to get the raw list of values, you can use `hart::collect()`.
    /// @param reducer Callable object that accepts a pair of std::vector iterators (begin and end).
    /// You'll probably want to use one of the HART's built-in reducers, see @ref Reducers.
    template <typename ReducerType>
    auto result (ReducerType&& reducer) const
        -> ReducerResultType<ReducerType, std::vector<double>::const_iterator>
    {
        return reducer (m_observedDurationsSeconds.begin(), m_observedDurationsSeconds.end());
    }

private:
    std::vector<double> m_observedDurationsSeconds;
};

/// @brief A helper class to measure time duration of some block of code 
/// @private
struct TimeItSample
{
    using clock = std::chrono::steady_clock;

    TimeIt& m_timeIt;
    clock::time_point m_start;

    explicit TimeItSample (TimeIt& timeIt) :
        m_timeIt (timeIt),
        m_start (clock::now())
    {
    }

    TimeItSample (const TimeItSample&) = delete;
    TimeItSample (TimeItSample&&) = delete;
    TimeItSample& operator= (const TimeItSample&) = delete;
    TimeItSample& operator= (TimeItSample&&) = delete;

    ~TimeItSample()
    {
        const clock::time_point end = clock::now();

        using std::chrono::duration_cast;
        using DurationDoubleType = std::chrono::duration<double>;
        const double durationSeconds = duration_cast<DurationDoubleType> (end - m_start).count();

        m_timeIt.add (durationSeconds);
    }
};

}  // namespace hart
