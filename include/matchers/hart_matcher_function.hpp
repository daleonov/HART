#pragma once

#include <functional>
#include <ostream>

#include "hart_condition.hpp"
#include "hart_exceptions.hpp"
#include "matchers/hart_matcher.hpp"

namespace hart
{

/// @brief Matcher defined by a user-provided function
/// @details This matcher allows defining custom matching logic using a callable object
/// (e.g. a lambda, function, or functor), instead of creating a dedicated matcher class.
/// The function can have one of the following signatures:
/// @code
/// Condition (const AudioBuffer<SampleType>& output);
/// Condition (const AudioBuffer<SampleType>& input, const AudioBuffer<SampleType>& output);
/// @endcode
///
/// Example:
/// @code
/// [] (const AudioBuffer<SampleType>& output) { return HART_LESS_THAN (crestFactorDb (output), 3_dB); }
/// @endcode
///
/// Return value should be a hart::Condition object, which you can create using one of the built-in macros,
/// like HART_FLOAT_EQUAL(), HART_LESS_THAN() etc. For the full list of available macros, see @ref Conditions.
///
/// The function may also return a regular boolean value, like so:
/// @code
/// [] (const AudioBuffer<SampleType>& output) { return crestFactorDb (output) < 3_dB; }
/// @endcode
///
/// In this case result will still be implicitly converted to Condition. However, it is encouraged to use proper
/// Condition macros, since returning a plain `bool` won't give you detailed traceback info if the test fails.
///
/// The matcher operates on the full rendered buffer and does not support
/// per-block evaluation.
/// @ingroup Matchers
template<typename SampleType>
class MatcherFunction:
    public Matcher<SampleType, MatcherFunction<SampleType>>
{
public:

    /// @brief Creates a matcher from a function that compares input and output
    /// @param matcherFunction A callable with signature:
    /// @code
    /// Condition (const AudioBuffer<SampleType>& input,
    ///       const AudioBuffer<SampleType>& output)
    /// @endcode
    /// It should return `true` if the output satisfies the expected condition, `false` otherwise
    /// @param label Optional human-readable label used in failure reports
    MatcherFunction (std::function <Condition (const AudioBuffer<SampleType>&, const AudioBuffer<SampleType>&)> matcherFunction, const std::string& label = {}) :
        m_matcherFunctionForInputAndOutput (std::move (matcherFunction)),
        m_label (label)
    {
    }

    /// @brief Constructs a matcher from a function that inspects only the output
    /// @param matcherFunction A callable with signature:
    /// @code
    /// Condition (const AudioBuffer<SampleType>& output)
    /// @endcode
    /// It should return `true` if the output satisfies the expected condition, `false` otherwise
    /// @param label Optional human-readable label used in failure reports
    MatcherFunction (std::function <Condition (const AudioBuffer<SampleType>&)> matcherFunction, const std::string& label = {}) :
        m_matcherFunctionForOutputOnly (std::move (matcherFunction)),
        m_label (label)
    {
    }

    bool match (const AudioBuffer<SampleType>& inputAudio, const AudioBuffer<SampleType>& observedOutputAudio) override
    {
        if (m_matcherFunctionForOutputOnly != nullptr)
        {
            m_condition = std::move (m_matcherFunctionForOutputOnly (observedOutputAudio));
            return m_condition.getResult();
        }

        if (m_matcherFunctionForInputAndOutput != nullptr)
        {
            m_condition = std::move (m_matcherFunctionForInputAndOutput (inputAudio, observedOutputAudio));
            return m_condition.getResult();
        }

        HART_THROW_OR_RETURN (hart::NullPointerError, "Matcher function is a nullptr!", false);
    }

    virtual MatcherFailureDetails getFailureDetails() const override
    {
        MatcherFailureDetails details;
        details.frame = 0;
        details.channel = 0;
        std::ostringstream descriptionStream;

        if (m_condition.hasDetailedMetadata())
        {
            hassert (m_condition.getLine() >= 0);
            hassert (m_condition.getFile() != nullptr);
            hassert (m_condition.getFile()[0] != '\0');

            descriptionStream
                << "Location: " << m_condition.getFile()
                << ':' << m_condition.getLine()
                << "\nCondition: ";
            m_condition.representWithTokens (descriptionStream);
            descriptionStream << "\nEvaluated: ";
            m_condition.representWithStringRepresentations (descriptionStream);
        }
        else
        {
            descriptionStream << "Matcher function (" << (m_label.empty() ? "no label" : m_label) << ") has evaluated to false";
        }

        details.description = descriptionStream.str();
        return details;
    }

    void represent (std::ostream& stream) const override
    {
        stream << "MatcherFunction (<function>, \"" << m_label << "\")";
    }

    bool canOperatePerBlock() const override { return false;}
    void prepare (double /* sampleRateHz */, size_t /* numChannels */, size_t /* maxBlockSizeFrames */) override {}

private:
    const std::function <Condition (const AudioBuffer<SampleType>&, const AudioBuffer<SampleType>&)> m_matcherFunctionForInputAndOutput = nullptr;
    const std::function <Condition (const AudioBuffer<SampleType>&)> m_matcherFunctionForOutputOnly = nullptr;
    const std::string m_label;
    Condition m_condition;
};

HART_MATCHER_DECLARE_ALIASES_FOR (MatcherFunction)

}  // namespace hart
