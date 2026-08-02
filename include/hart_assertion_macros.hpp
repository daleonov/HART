#pragma once

#include "hart_assertion.hpp"
#include "hart_assertion_dispatcher.hpp"
#include "hart_condition_macros.hpp"

// TODO: Document each of those macros

/// @defgroup FreeStandingAssertions Free-Standing Assertions
/// @brief Generic assertions for HART test cases
/// @{

#define HART_EXPECT(condition) \
    ::hart::AssertionDispatcher (::hart::Assertion::makeExpect ((condition), __FILE__, __LINE__))

#define HART_ASSERT(condition) \
    ::hart::AssertionDispatcher (::hart::Assertion::makeAssert ((condition), __FILE__, __LINE__))

#define HART_EXPECT_TRUE(value) \
    HART_EXPECT (HART_TRUE (value))

#define HART_ASSERT_TRUE(value) \
    HART_ASSERT (HART_TRUE (value))

#define HART_EXPECT_FALSE(value) \
    HART_EXPECT (HART_FALSE (value))

#define HART_ASSERT_FALSE(value) \
    HART_ASSERT (HART_FALSE (value))

#define HART_EXPECT_EQUAL(lhs, rhs) \
    HART_EXPECT (HART_EQUAL (lhs, rhs))

#define HART_ASSERT_EQUAL(lhs, rhs) \
    HART_ASSERT (HART_EQUAL (lhs, rhs))

#define HART_EXPECT_EQ(lhs, rhs) \
    HART_EXPECT_EQUAL (lhs, rhs)

#define HART_ASSERT_EQ(lhs, rhs) \
    HART_ASSERT_EQUAL (lhs, rhs)

#define HART_EXPECT_NOT_EQUAL(lhs, rhs) \
    HART_EXPECT (HART_NOT_EQUAL (lhs, rhs))

#define HART_ASSERT_NOT_EQUAL(lhs, rhs) \
    HART_ASSERT (HART_NOT_EQUAL (lhs, rhs))

#define HART_EXPECT_NE(lhs, rhs) \
    HART_EXPECT_NOT_EQUAL (lhs, rhs)

#define HART_ASSERT_NE(lhs, rhs) \
    HART_ASSERT_NOT_EQUAL (lhs, rhs)

#define HART_EXPECT_FLOAT_EQUAL(lhs, rhs, tolerance) \
    HART_EXPECT (HART_FLOAT_EQUAL (lhs, rhs, tolerance))

#define HART_ASSERT_FLOAT_EQUAL(lhs, rhs, tolerance) \
    HART_ASSERT (HART_FLOAT_EQUAL (lhs, rhs, tolerance))

#define HART_EXPECT_FLOAT_EQ(lhs, rhs, tolerance) \
    HART_EXPECT_FLOAT_EQUAL (lhs, rhs, tolerance)

#define HART_ASSERT_FLOAT_EQ(lhs, rhs, tolerance) \
    HART_ASSERT_FLOAT_EQUAL (lhs, rhs, tolerance)

#define HART_EXPECT_FLOAT_NOT_EQUAL(lhs, rhs, tolerance) \
    HART_EXPECT (HART_FLOAT_NOT_EQUAL (lhs, rhs, tolerance))

#define HART_ASSERT_FLOAT_NOT_EQUAL(lhs, rhs, tolerance) \
    HART_ASSERT (HART_FLOAT_NOT_EQUAL (lhs, rhs, tolerance))

#define HART_EXPECT_FLOAT_NE(lhs, rhs, tolerance) \
    HART_EXPECT_FLOAT_NOT_EQUAL (lhs, rhs, tolerance)

#define HART_ASSERT_FLOAT_NE(lhs, rhs, tolerance) \
    HART_ASSERT_FLOAT_NOT_EQUAL (lhs, rhs, tolerance)

#define HART_EXPECT_FREQUENCIES_EQUAL(observedFrequencyHz, expectedFrequencyHz, toleranceCents) \
    HART_EXPECT (HART_FREQUENCIES_EQUAL(observedFrequencyHz, expectedFrequencyHz, toleranceCents))

#define HART_ASSERT_FREQUENCIES_EQUAL(observedFrequencyHz, expectedFrequencyHz, toleranceCents) \
    HART_ASSERT (HART_FREQUENCIES_EQUAL(observedFrequencyHz, expectedFrequencyHz, toleranceCents))

#define HART_EXPECT_FREQ_EQ(observedFrequencyHz, expectedFrequencyHz, toleranceCents) \
    HART_EXPECT_FREQUENCIES_EQUAL(observedFrequencyHz, expectedFrequencyHz, toleranceCents)

#define HART_ASSERT_FREQ_EQ(observedFrequencyHz, expectedFrequencyHz, toleranceCents) \
    HART_ASSERT_FREQUENCIES_EQUAL(observedFrequencyHz, expectedFrequencyHz, toleranceCents)

#define HART_EXPECT_FREQUENCIES_NOT_EQUAL(observedFrequencyHz, expectedFrequencyHz, toleranceCents) \
    HART_EXPECT (HART_FREQUENCIES_NOT_EQUAL(observedFrequencyHz, expectedFrequencyHz, toleranceCents))

#define HART_ASSERT_FREQUENCIES_NOT_EQUAL(observedFrequencyHz, expectedFrequencyHz, toleranceCents) \
    HART_ASSERT (HART_FREQUENCIES_NOT_EQUAL(observedFrequencyHz, expectedFrequencyHz, toleranceCents))

#define HART_EXPECT_FREQ_NE(observedFrequencyHz, expectedFrequencyHz, toleranceCents) \
    HART_EXPECT_FREQUENCIES_NOT_EQUAL(observedFrequencyHz, expectedFrequencyHz, toleranceCents)

#define HART_ASSERT_FREQ_NE(observedFrequencyHz, expectedFrequencyHz, toleranceCents) \
    HART_ASSERT_FREQUENCIES_NOT_EQUAL(observedFrequencyHz, expectedFrequencyHz, toleranceCents)

#define HART_EXPECT_GREATER_THAN(lhs, rhs) \
    HART_EXPECT (HART_GREATER_THAN (lhs, rhs))

#define HART_ASSERT_GREATER_THAN(lhs, rhs) \
    HART_ASSERT (HART_GREATER_THAN (lhs, rhs))

#define HART_EXPECT_GT(lhs, rhs) \
    HART_EXPECT_GREATER_THAN (lhs, rhs)

#define HART_ASSERT_GT(lhs, rhs) \
    HART_ASSERT_GREATER_THAN (lhs, rhs)

#define HART_EXPECT_GREATER_OR_EQUAL(lhs, rhs) \
    HART_EXPECT (HART_GREATER_OR_EQUAL (lhs, rhs))

#define HART_ASSERT_GREATER_OR_EQUAL(lhs, rhs) \
    HART_ASSERT (HART_GREATER_OR_EQUAL (lhs, rhs))

#define HART_EXPECT_GE(lhs, rhs) \
    HART_EXPECT_GREATER_OR_EQUAL (lhs, rhs)

#define HART_ASSERT_GE(lhs, rhs) \
    HART_ASSERT_GREATER_OR_EQUAL (lhs, rhs)

#define HART_EXPECT_IS_NAN(value) \
    HART_EXPECT (HART_IS_NAN (value))

#define HART_ASSERT_IS_NAN(value) \
    HART_ASSERT (HART_IS_NAN (value))

#define HART_EXPECT_NOT_NAN(value) \
    HART_EXPECT (HART_NOT_NAN (value))

#define HART_ASSERT_NOT_NAN(value) \
    HART_ASSERT (HART_NOT_NAN (value))

#define HART_EXPECT_LESS_THAN(lhs, rhs) \
    HART_EXPECT (HART_LESS_THAN (lhs, rhs))

#define HART_ASSERT_LESS_THAN(lhs, rhs) \
    HART_ASSERT (HART_LESS_THAN (lhs, rhs))

#define HART_EXPECT_LT(lhs, rhs) \
    HART_EXPECT_LESS_THAN (lhs, rhs)

#define HART_ASSERT_LT(lhs, rhs) \
    HART_ASSERT_LESS_THAN (lhs, rhs)

#define HART_EXPECT_LESS_OR_EQUAL(lhs, rhs) \
    HART_EXPECT (HART_LESS_OR_EQUAL (lhs, rhs))

#define HART_ASSERT_LESS_OR_EQUAL(lhs, rhs) \
    HART_ASSERT (HART_LESS_OR_EQUAL (lhs, rhs))

#define HART_EXPECT_LE(lhs, rhs) \
    HART_EXPECT_LESS_OR_EQUAL (lhs, rhs)

#define HART_ASSERT_LE(lhs, rhs) \
    HART_ASSERT_LESS_OR_EQUAL (lhs, rhs)

#define HART_EXPECT_IN_RANGE(value, minValue, maxValue) \
    HART_EXPECT (HART_IN_RANGE (value, minValue, maxValue))

#define HART_ASSERT_IN_RANGE(value, minValue, maxValue) \
    HART_ASSERT (HART_IN_RANGE (value, minValue, maxValue))

#define HART_EXPECT_FLOAT_IN_RANGE(value, minValue, maxValue, tolerance) \
    HART_EXPECT (HART_FLOAT_IN_RANGE (value, minValue, maxValue, tolerance))

#define HART_ASSERT_FLOAT_IN_RANGE(value, minValue, maxValue, tolerance) \
    HART_ASSERT (HART_FLOAT_IN_RANGE (value, minValue, maxValue, tolerance))

/// @}
