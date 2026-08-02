#pragma once

#include "hart_condition.hpp"

// TODO: Document each of those macros

/// @defgroup Conditions Conditions
/// @brief Rich boolean conditions for free-standing assertions and function-based matchers
/// @{

#define HART_TRUE(value) \
    ::hart::Condition::truth ((value), #value, __FILE__, __LINE__)

#define HART_FALSE(value) \
    ::hart::Condition::falsehood ((value), #value, __FILE__, __LINE__)

#define HART_EQUAL(lhs, rhs) \
    ::hart::Condition::equals ((lhs), (rhs), #lhs, #rhs, __FILE__, __LINE__)

#define HART_EQ(lhs, rhs) \
    HART_EQUAL (lhs, rhs)

#define HART_NOT_EQUAL(lhs, rhs) \
    ::hart::Condition::notEquals ((lhs), (rhs), #lhs, #rhs, __FILE__, __LINE__)

#define HART_NE(lhs, rhs) \
    HART_NOT_EQUAL (lhs, rhs)

#define HART_FLOAT_EQUAL(lhs, rhs, tolerance) \
    ::hart::Condition::floatEqual ((lhs), (rhs), (tolerance), #lhs, #rhs, #tolerance, __FILE__, __LINE__)

#define HART_FLOAT_EQ(lhs, rhs, tolerance) \
    HART_FLOAT_EQUAL (lhs, rhs, tolerance)

#define HART_FLOAT_NOT_EQUAL(lhs, rhs, tolerance) \
    ::hart::Condition::floatNotEqual ((lhs), (rhs), (tolerance), #lhs, #rhs, #tolerance, __FILE__, __LINE__)

#define HART_FLOAT_NE(lhs, rhs, tolerance) \
    HART_FLOAT_NOT_EQUAL (lhs, rhs, tolerance)

#define HART_FREQUENCIES_EQUAL(observedFrequencyHz, expectedFrequencyHz, toleranceCents) \
    ::hart::Condition::frequenciesEqual ((observedFrequencyHz), (expectedFrequencyHz), (toleranceCents), #observedFrequencyHz, #expectedFrequencyHz, #toleranceCents, __FILE__, __LINE__)

#define HART_FREQ_EQ(observedFrequencyHz, expectedFrequencyHz, toleranceCents) \
    HART_FREQUENCIES_EQUAL(observedFrequencyHz, expectedFrequencyHz, toleranceCents)

#define HART_FREQUENCIES_NOT_EQUAL(observedFrequencyHz, expectedFrequencyHz, toleranceCents) \
    ::hart::Condition::frequenciesNotEqual ((observedFrequencyHz), (expectedFrequencyHz), (toleranceCents), #observedFrequencyHz, #expectedFrequencyHz, #toleranceCents, __FILE__, __LINE__)

#define HART_FREQ_NE(observedFrequencyHz, expectedFrequencyHz, toleranceCents) \
    HART_FREQUENCIES_NOT_EQUAL(observedFrequencyHz, expectedFrequencyHz, toleranceCents)

#define HART_GREATER_THAN(lhs, rhs) \
    ::hart::Condition::greaterThan ((lhs), (rhs), #lhs, #rhs, __FILE__, __LINE__)

#define HART_GT(lhs, rhs) \
    HART_GREATER_THAN (lhs, rhs)

#define HART_GREATER_OR_EQUAL(lhs, rhs) \
    ::hart::Condition::greaterOrEqual ((lhs), (rhs), #lhs, #rhs, __FILE__, __LINE__)

#define HART_GE(lhs, rhs) \
    HART_GREATER_OR_EQUAL (lhs, rhs)

#define HART_LESS_THAN(lhs, rhs) \
    ::hart::Condition::lessThan ((lhs), (rhs), #lhs, #rhs, __FILE__, __LINE__)

#define HART_LT(lhs, rhs) \
    HART_LESS_THAN (lhs, rhs)

#define HART_LESS_OR_EQUAL(lhs, rhs) \
    ::hart::Condition::lessOrEqual ((lhs), (rhs), #lhs, #rhs, __FILE__, __LINE__)

#define HART_LE(lhs, rhs) \
    HART_LESS_OR_EQUAL (lhs, rhs)

#define HART_IN_RANGE(value, minValue, maxValue) \
    ::hart::Condition::inRange ((value), (minValue), (maxValue), #value, #minValue, #maxValue, __FILE__, __LINE__)

#define HART_FLOAT_IN_RANGE(value, minValue, maxValue, tolerance) \
    ::hart::Condition::floatInRange ((value), (minValue), (maxValue), (tolerance), #value, #minValue, #maxValue, #tolerance, __FILE__, __LINE__)

#define HART_IS_NAN(value) \
    ::hart::Condition::isNaN ((value), #value, __FILE__, __LINE__)

#define HART_NOT_NAN(value) \
    ::hart::Condition::notNaN ((value), #value, __FILE__, __LINE__)

/// @}
