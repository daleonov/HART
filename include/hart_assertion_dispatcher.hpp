#pragma once

#include <ostream>
#include <sstream>
#include <string>
#include <utility>

#include "hart_assertion.hpp"
#include "hart_expectation_failure_messages.hpp"
#include "hart_exceptions.hpp"
#include "hart_utils.hpp"

namespace hart
{

/// @brief Communicates the assertion's result to the test runner
/// @details Meant to be instantiated via @FreeStandingAssertions
/// macros, like `HART_EXPECT_EQUAL()`. Also, supplies the "<<"
/// interface for appending extra label, for example:
/// @code
/// HART_EXPECT_EQUAL (a, b) << "Some label " << someValue;
/// @endcode
/// @private
class AssertionDispatcher
{
public:
    explicit AssertionDispatcher (Assertion assertion) :
        m_assertion (std::move (assertion))
    {
        if (! m_assertion.hasFailed())
            return;

        m_messageStream
            << m_assertion.getFile() << ':' << m_assertion.getLine() << '\n'
            << (m_assertion.getLevel() == Assertion::Level::expect ? "HART_EXPECT()" : "HART_ASSERT()")
            << " failed: \nCondition: ";
        m_assertion.representWithTokens (m_messageStream);
        m_messageStream << "\nEvaluated: ";
        m_assertion.representWithStringRepresentations (m_messageStream);
    }

    AssertionDispatcher (const AssertionDispatcher&) = delete;
    AssertionDispatcher (AssertionDispatcher&&) = delete;
    AssertionDispatcher& operator= (const AssertionDispatcher&) = delete;
    AssertionDispatcher& operator= (AssertionDispatcher&&) = delete;

    template <typename Type>
    AssertionDispatcher& operator<< (const Type& value)
    {
        if (m_assertion.hasFailed())
        {
            if (! m_hasUserLabel)
            {
                m_hasUserLabel = true;
                m_messageStream << "\nLabel: \"";
            }

            m_messageStream << value;
        }

        return *this;
    }

    ~AssertionDispatcher() noexcept(false)
    {
        if (! m_assertion.hasFailed())
            return;

        if (m_hasUserLabel)
            m_messageStream << '\"';

        const std::string message = m_messageStream.str();

        if (! isExceptionUnwinding() && m_assertion.getLevel() == Assertion::Level::assert)
            throw hart::TestAssertException (message);

        ExpectationFailureMessages::get().emplace_back (message);
    }

private:
    const Assertion m_assertion;
    bool m_hasUserLabel = false;
    std::ostringstream m_messageStream;
};

}  // namespace hart
