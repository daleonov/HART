#pragma once

#include <cstddef>
#include <string>
#include <ostream>
#include <type_traits>
#include <utility>
#include <vector>

#include "hart_exceptions.hpp"

// TODO: Document this macro
#define HART_GENERATE_VALUE(...) \
    ::hart::ActiveParametrisedTaskContext::get().values( \
        __FILE__, \
        __LINE__, \
        ::hart::makeParametrisedValueSet(__VA_ARGS__))

namespace hart
{

struct ParametrisedValuesSlot
{
    std::string file;
    int line;
    size_t size;
};

template <typename ValueType>
class ParametrisedValueSequence
{
public:
    using ResolvedValueType = ValueType;

    explicit ParametrisedValueSequence (std::vector<ValueType> values):
        m_values (std::move (values))
    {
    }

    size_t size() const
    {
        return m_values.size();
    }

    const ValueType& at (const size_t index) const
    {
        return m_values.at (index);
    }

private:
    std::vector<ValueType> m_values;
};

/// @brief Value sequence factory for variadic arguments, as in `HART_GEVERATE_VALUE (11, 22, 33)`
/// @private
template <typename FirstValueType, typename... OtherValueTypes>
auto makeParametrisedValueSet (FirstValueType&& firstValue, OtherValueTypes&&... otherValues)
    -> ParametrisedValueSequence<typename std::common_type<typename std::decay<FirstValueType>::type, typename std::decay<OtherValueTypes>::type...>::type>
{
    using ResolvedValueType = typename std::common_type<typename std::decay<FirstValueType>::type, typename std::decay<OtherValueTypes>::type...>::type;
    std::vector<ResolvedValueType> values;
    values.reserve (1 + sizeof... (OtherValueTypes));
    values.push_back (static_cast<ResolvedValueType> (std::forward<FirstValueType> (firstValue)));

    // A trick to push_back() each of the otherValues items into the vector 
    const int dummy[] = { 42, (values.push_back (static_cast<ResolvedValueType> (std::forward<OtherValueTypes> (otherValues))), 42)... };
    (void) dummy;

    return ParametrisedValueSequence<ResolvedValueType> (std::move (values));
}

/// @brief Value sequence factory for a pair of iterators, as in `HART_GEVERATE_VALUE (x.begin(), x.end())`
/// @private
template <typename IteratorType>
auto makeParametrisedValueSet (IteratorType begin, IteratorType end)
    -> ParametrisedValueSequence<typename std::iterator_traits<IteratorType>::value_type>
{
    using ValueType = typename std::iterator_traits<IteratorType>::value_type;
    return ParametrisedValueSequence<ValueType> (std::vector<ValueType> (begin, end));
}

/// @brief A helper to determine if something is an iterable container with `begin()` and `end()` methods
/// @private 
template <typename ValueType>
class IsIterable
{
private:
    template <typename CandidateType>
    static auto test (int) -> decltype (
        std::declval<const CandidateType&>().begin(),
        std::declval<const CandidateType&>().end(),
        typename CandidateType::value_type(),
        std::true_type()
    );

    template <typename>
    static std::false_type test (...);

public:
    static const bool value = decltype (test<ValueType> (42))::value;
};

/// @brief Value sequence factory for a container, as in `HART_GENERATE_VALUE (someVector)`
/// @private 
template <typename IterableType>
typename std::enable_if<
    IsIterable<IterableType>::value,
    ParametrisedValueSequence<typename IterableType::value_type>
>::type
makeParametrisedValueSet (const IterableType& iterable)
{
    return makeParametrisedValueSet (iterable.begin(), iterable.end());
}

class ParametrisedTaskContext
{
public:
    template <typename ValueSequenceType>
    auto values (const char* file, int line, ValueSequenceType&& sequence)
        -> typename std::decay<ValueSequenceType>::type::ResolvedValueType
    {
        if (sequence.size() == 0)
            HART_THROW_OR_RETURN (SizeError, "HART_GENERATE_VALUE() received an empty value sequence", typename std::decay<ValueSequenceType>::type::ResolvedValueType());

        const size_t slotIndex = m_cursor;

        if (m_isFirstPermutation)
        {
            m_parametrisedValuesSlots.push_back (ParametrisedValuesSlot { file, line, sequence.size() });
            m_currentIndices.push_back (0);
        }
        else
        {
            // HART_GENERATE_VALUE() call sites changed between value permutations?
            hassert (slotIndex < m_parametrisedValuesSlots.size());

            const ParametrisedValuesSlot& slot = m_parametrisedValuesSlots[slotIndex];

            // HART_GENERATE_VALUE() call sites changed between value permutations?
            hassert (slot.file == file && slot.line == line)

            if (slot.size != sequence.size())
                HART_THROW_OR_RETURN (hart::ConfigurationError, "HART_GENERATE_VALUE() sequence size changed between value permutations", ResolvedValueType());
        }

        ++m_cursor;
        return sequence.at (m_currentIndices[slotIndex]);
    }

    bool hasUnusedValuePermutations() const
    {
        return ! m_isExhausted;
    }

    void beginPermutation()
    {
        m_cursor = 0;
    }

    void endPermutation()
    {
        // HART_GENERATE_VALUES() call sites changed between value permutations?
        hassert (m_isFirstPermutation || m_cursor == m_parametrisedValuesSlots.size());

        m_isFirstPermutation = false;
    }

    void advanceToNextValuePermutation()
    {
        if (m_parametrisedValuesSlots.empty())
        {
            m_isExhausted = true;
            return;
        }

        for (size_t i = m_currentIndices.size(); i > 0; --i)
        {
            const size_t index = i - 1;
            ++m_currentIndices[index];

            if (m_currentIndices[index] < m_parametrisedValuesSlots[index].size)
                return;

            m_currentIndices[index] = 0;
        }

        m_isExhausted = true;
    }

private:
    std::vector<ParametrisedValuesSlot> m_parametrisedValuesSlots;
    std::vector<size_t> m_currentIndices;
    size_t m_cursor = 0;
    bool m_isFirstPermutation = true;
    bool m_isExhausted = false;
};

class ActiveParametrisedTaskContext
{
public:
    static ParametrisedTaskContext& get()
    {
        if (currentContext == nullptr)
        {
            static ParametrisedTaskContext fallbackContext;

            HART_THROW_OR_RETURN (
                hart::ConfigurationError,
                "HART_GENERATE_VALUE() can only be used inside a parametrised task - use HART_PARAMETRISED_TEST(), or another macro from the parametrised family",
                fallbackContext
            );
        }

        return *currentContext;
    }

    static bool hasActiveContext()
    {
        return currentContext != nullptr;
    }

    static thread_local ParametrisedTaskContext* currentContext;
};

class ParametrisedTaskContextScope
{
public:
    explicit ParametrisedTaskContextScope (ParametrisedTaskContext& context)
    {
        m_previousContext = ActiveParametrisedTaskContext::currentContext;
        ActiveParametrisedTaskContext::currentContext = &context;
    }

    ~ParametrisedTaskContextScope()
    {
        ActiveParametrisedTaskContext::currentContext = m_previousContext;
    }

private:
    ParametrisedTaskContext* m_previousContext = nullptr;
};

#if defined (HART_IMPLEMENTATION)
thread_local ParametrisedTaskContext* ActiveParametrisedTaskContext::currentContext = nullptr;
#endif

}  // namespace hart
