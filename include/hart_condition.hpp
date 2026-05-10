#pragma once

#include <ostream>
#include <sstream>
#include <string>
#include <type_traits>
#include <utility>

#include "hart_stringify.hpp"
#include "hart_utils.hpp"  // addCents()

namespace hart
{

/// @brief A class representing some condition
/// @details This condition id intended to be used by matcher function or a free-standing assertion macro.
/// To instantiate, use macros like `HART_EQUAL()` or `HART_FLOAT_IN_RANGE()` - see @ref Conditions.
/// @ingroup Conditions
class Condition
{
public:
    /// @private
    Condition (bool result):
        Condition (
            result,
            "Condition (<bool>)",  // tokenRepresentation
            "Condition (" + toString (result) + ')',  // stringRepresentation
            "",  // file
            -1,  // line
            false  // hasDetailedMetadata
            )
    {
    }

    /// @private
    Condition():
        Condition (
            false,  // result
            "Condition()",  // tokenRepresentation
            "Condition()",  // stringRepresentation
            "",  // file
            -1,  // line
            false  // hasDetailedMetadata
            )
    {
    }

    /// @private
    bool getResult() const
    {
        return m_result;
    }

    /// @private
    const char* getFile() const
    {
        return m_file;
    }

    /// @private
    int getLine() const
    {
        return m_line;
    }

    /// @brief Returns whether it has rich metadata that's worth displaying on failure.
    /// @details
    /// Conditions created via proper macros have some useful diagnostic metadata
    /// like token representation, or file name and line number where it was defined.
    /// Default-constructed Conditions and Conditions created from basic `bool` value
    /// do not have it.
    /// @private
    bool hasDetailedMetadata() const
    {
        return m_hasDetailedMetadata;
    }

    /// @private
    void representWithTokens (std::ostream& stream) const
    {
        stream << m_tokenRepresentation;
    }

    /// @private
    void representWithStringRepresentations (std::ostream& stream) const
    {
        stream << m_stringRepresentation;
    }

    /// @TODO: Enforce explicit booleans here?
    /// @private
    template <typename ValueType>
    static Condition truth (ValueType&& value, const char* valueTokens, const char* file, int line)
    {
        std::ostringstream tokenRepresentationStream;
        std::ostringstream stringRepresentationStream;
        tokenRepresentationStream << "HART_TRUE (" << valueTokens << ")";
        stringRepresentationStream << "HART_TRUE (" << hart::toString (value) << ")";
        return Condition (static_cast<bool> (value), tokenRepresentationStream.str(), stringRepresentationStream.str(), file, line);
    }

    /// @TODO: Enforce explicit booleans here?
    /// @private
    template <typename ValueType>
    static Condition falsehood (ValueType&& value, const char* valueTokens, const char* file, int line)
    {
        std::ostringstream tokenRepresentationStream;
        std::ostringstream stringRepresentationStream;
        tokenRepresentationStream << "HART_FALSE (" << valueTokens << ")";
        stringRepresentationStream << "HART_FALSE (" << hart::toString (value) << ")";
        return Condition (! static_cast<bool> (value), tokenRepresentationStream.str(), stringRepresentationStream.str(), file, line);
    }

    /// @TODO: Do a warning if used with floats?
    /// @private
    template <typename LHSType, typename RHSType>
    static Condition equals (LHSType&& lhs, RHSType&& rhs, const char* lhsTokens, const char* rhsTokens, const char* file, int line)
    {
        std::ostringstream tokenRepresentationStream;
        std::ostringstream stringRepresentationStream;
        tokenRepresentationStream << "HART_EQUAL (" << lhsTokens << ", " << rhsTokens << ")";
        stringRepresentationStream << "HART_EQUAL (" << hart::toString (lhs) << ", " << hart::toString (rhs) << ")";
        return Condition (lhs == rhs, tokenRepresentationStream.str(), stringRepresentationStream.str(), file, line);
    }

    /// @TODO: Do a warning if used with floats?
    /// @private
    template <typename LHSType, typename RHSType>
    static Condition notEquals (LHSType&& lhs, RHSType&& rhs, const char* lhsTokens, const char* rhsTokens, const char* file, int line)
    {
        std::ostringstream tokenRepresentationStream;
        std::ostringstream stringRepresentationStream;
        tokenRepresentationStream << "HART_NOT_EQUAL (" << lhsTokens << ", " << rhsTokens << ")";
        stringRepresentationStream << "HART_NOT_EQUAL (" << hart::toString (lhs) << ", " << hart::toString (rhs) << ")";
        return Condition (lhs != rhs, tokenRepresentationStream.str(), stringRepresentationStream.str(), file, line);
    }

    /// @private
    template <typename FloatType>
    static typename std::enable_if<std::is_floating_point<FloatType>::value, Condition>::type
    floatEqual (
        FloatType lhs,
        FloatType rhs,
        FloatType tolerance,
        const char* lhsTokens,
        const char* rhsTokens,
        const char* toleranceTokens,
        const char* file,
        int line
        )
    {
        std::ostringstream tokenRepresentationStream;
        std::ostringstream stringRepresentationStream;
        tokenRepresentationStream << "HART_FLOAT_EQUAL (" << lhsTokens << ", " << rhsTokens << ", " << toleranceTokens << ")";
        stringRepresentationStream << "HART_FLOAT_EQUAL (" << hart::toString (lhs) << ", " << hart::toString (rhs) << ", " << hart::toString (tolerance) << ")";
        return Condition (
            lhs - tolerance <= rhs && rhs <= lhs + tolerance,
            tokenRepresentationStream.str(),
            stringRepresentationStream.str(),
            file,
            line
            );
    }

    /// @private
    template <typename FloatType>
    static typename std::enable_if<std::is_floating_point<FloatType>::value, Condition>::type
    floatNotEqual (
        FloatType lhs,
        FloatType rhs,
        FloatType tolerance,
        const char* lhsTokens,
        const char* rhsTokens,
        const char* toleranceTokens,
        const char* file,
        int line
        )
    {
        std::ostringstream tokenRepresentationStream;
        std::ostringstream stringRepresentationStream;
        tokenRepresentationStream << "HART_FLOAT_NOT_EQUAL (" << lhsTokens << ", " << rhsTokens << ", " << toleranceTokens << ")";
        stringRepresentationStream << "HART_FLOAT_NOT_EQUAL (" << hart::toString (lhs) << ", " << hart::toString (rhs) << ", " << hart::toString (tolerance) << ")";
        return Condition (
            lhs - tolerance > rhs || rhs > lhs + tolerance,
            tokenRepresentationStream.str(),
            stringRepresentationStream.str(),
            file,
            line
            );
    }

    ///@private
    static Condition
    frequenciesEqual (
        double observedFrequencyHz,
        double expectedFrequencyHz,
        double toleranceCents,
        const char* observedFrequencyTokens,
        const char* expectedFrequencyTokens,
        const char* toleranceTokens,
        const char* file,
        int line
        )
    {
        std::ostringstream tokenRepresentationStream;
        std::ostringstream stringRepresentationStream;
        tokenRepresentationStream << "HART_FREQUENCIES_EQUAL (" << observedFrequencyTokens << ", " << expectedFrequencyTokens << ", " << toleranceTokens << ')';
        stringRepresentationStream << "HART_FREQUENCIES_EQUAL (" << hart::toString (observedFrequencyHz) << ", " << hart::toString (expectedFrequencyHz) << ", " << hart::toString (toleranceCents) << ')';
        return Condition (
            (observedFrequencyHz >= addCents (expectedFrequencyHz, -toleranceCents)) && (observedFrequencyHz <= addCents (expectedFrequencyHz, toleranceCents)),
            tokenRepresentationStream.str(),
            stringRepresentationStream.str(),
            file,
            line
            );
    }

    ///@private
    static Condition
    frequenciesNotEqual (
        double observedFrequencyHz,
        double expectedFrequencyHz,
        double toleranceCents,
        const char* observedFrequencyTokens,
        const char* expectedFrequencyTokens,
        const char* toleranceTokens,
        const char* file,
        int line
        )
    {
        std::ostringstream tokenRepresentationStream;
        std::ostringstream stringRepresentationStream;
        tokenRepresentationStream << "HART_FREQUENCIES_NOT_EQUAL (" << observedFrequencyTokens << ", " << expectedFrequencyTokens << ", " << toleranceTokens << ')';
        stringRepresentationStream << "HART_FREQUENCIES_NOT_EQUAL (" << hart::toString (observedFrequencyHz) << ", " << hart::toString (expectedFrequencyHz) << ", " << hart::toString (toleranceCents) << ')';
        return Condition (
            ! ((observedFrequencyHz >= addCents (expectedFrequencyHz, -toleranceCents)) && (observedFrequencyHz <= addCents (expectedFrequencyHz, toleranceCents))),
            tokenRepresentationStream.str(),
            stringRepresentationStream.str(),
            file,
            line
            );
    }

    /// @private
    template <typename LHSType, typename RHSType>
    static Condition greaterThan (LHSType&& lhs, RHSType&& rhs, const char* lhsTokens, const char* rhsTokens, const char* file, int line)
    {
        std::ostringstream tokenRepresentationStream;
        std::ostringstream stringRepresentationStream;
        tokenRepresentationStream << "HART_GREATER_THAN (" << lhsTokens << ", " << rhsTokens << ")";
        stringRepresentationStream << "HART_GREATER_THAN (" << hart::toString (lhs) << ", " << hart::toString (rhs) << ")";
        return Condition (lhs > rhs, tokenRepresentationStream.str(), stringRepresentationStream.str(), file, line);
    }

    /// @private
    template <typename LHSType, typename RHSType>
    static Condition greaterOrEqual (LHSType&& lhs, RHSType&& rhs, const char* lhsTokens, const char* rhsTokens, const char* file, int line)
    {
        std::ostringstream tokenRepresentationStream;
        std::ostringstream stringRepresentationStream;
        tokenRepresentationStream << "HART_GREATER_OR_EQUAL (" << lhsTokens << ", " << rhsTokens << ")";
        stringRepresentationStream << "HART_GREATER_OR_EQUAL (" << hart::toString (lhs) << ", " << hart::toString (rhs) << ")";
        return Condition (lhs >= rhs, tokenRepresentationStream.str(), stringRepresentationStream.str(), file, line);
    }

    /// @private
    template <typename LHSType, typename RHSType>
    static Condition lessThan (LHSType&& lhs, RHSType&& rhs, const char* lhsTokens, const char* rhsTokens, const char* file, int line)
    {
        std::ostringstream tokenRepresentationStream;
        std::ostringstream stringRepresentationStream;
        tokenRepresentationStream << "HART_LESS_THAN (" << lhsTokens << ", " << rhsTokens << ")";
        stringRepresentationStream << "HART_LESS_THAN (" << hart::toString (lhs) << ", " << hart::toString (rhs) << ")";
        return Condition (lhs < rhs, tokenRepresentationStream.str(), stringRepresentationStream.str(), file, line);
    }

    /// @private
    template <typename LHSType, typename RHSType>
    static Condition lessOrEqual (LHSType&& lhs, RHSType&& rhs, const char* lhsTokens, const char* rhsTokens, const char* file, int line)
    {
        std::ostringstream tokenRepresentationStream;
        std::ostringstream stringRepresentationStream;
        tokenRepresentationStream << "HART_LESS_OR_EQUAL (" << lhsTokens << ", " << rhsTokens << ")";
        stringRepresentationStream << "HART_LESS_OR_EQUAL (" << hart::toString (lhs) << ", " << hart::toString (rhs) << ")";
        return Condition (lhs <= rhs, tokenRepresentationStream.str(), stringRepresentationStream.str(), file, line);
    }

    /// @private
    template <typename ValueType, typename BoundType>
    static Condition inRange (
        ValueType&& value,
        BoundType&& minValue,
        BoundType&& maxValue,
        const char* valueTokens,
        const char* minValueTokens,
        const char* maxValueTokens,
        const char* file,
        int line
        )
    {
        std::ostringstream tokenRepresentationStream;
        std::ostringstream stringRepresentationStream;
        tokenRepresentationStream << "HART_IN_RANGE (" << valueTokens << ", " << minValueTokens << ", " << maxValueTokens << ")";
        stringRepresentationStream << "HART_IN_RANGE (" << hart::toString (value) << ", " << hart::toString (minValue) << ", " << hart::toString (maxValue) << ")";
        return Condition (minValue <= value && value <= maxValue, tokenRepresentationStream.str(), stringRepresentationStream.str(), file, line);
    }

    /// @private
    template <typename FloatType>
    static typename std::enable_if<std::is_floating_point<FloatType>::value, Condition>::type
    floatInRange (
        FloatType value,
        FloatType minValue,
        FloatType maxValue,
        FloatType tolerance,
        const char* valueTokens,
        const char* minValueTokens,
        const char* maxValueTokens,
        const char* toleranceTokens,
        const char* file,
        int line
        )
    {
        std::ostringstream tokenRepresentationStream;
        std::ostringstream stringRepresentationStream;
        tokenRepresentationStream << "HART_FLOAT_IN_RANGE (" << valueTokens << ", " << minValueTokens << ", " << maxValueTokens << ", " << toleranceTokens << ")";
        stringRepresentationStream << "HART_FLOAT_IN_RANGE (" << hart::toString (value) << ", " << hart::toString (minValue) << ", " << hart::toString (maxValue) << ", " << hart::toString (tolerance) << ")";
        return Condition (
            minValue - tolerance <= value && value <= maxValue + tolerance,
            tokenRepresentationStream.str(),
            stringRepresentationStream.str(),
            file,
            line
            );
    }

private:
    Condition (bool result, std::string tokenRepresentation, std::string stringRepresentation, const char* file, int line, bool hasDetailedMetadata = true) :
        m_result (result),
        m_hasDetailedMetadata (hasDetailedMetadata),
        m_tokenRepresentation (std::move (tokenRepresentation)),
        m_stringRepresentation (std::move (stringRepresentation)),
        m_file (file),
        m_line (line)
    {
    }

    bool m_result;
    bool m_hasDetailedMetadata;
    std::string m_tokenRepresentation;
    std::string m_stringRepresentation;
    const char* m_file;
    int m_line;
};

}  // namespace hart
