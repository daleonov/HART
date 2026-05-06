#pragma once

// Define this macro if those literals clash with existing ones in your codebase
#ifndef HART_DO_NOT_ADD_UNITS

#include "hart_utils.hpp"  // pi

/// @defgroup Units Units
/// @brief Better readability
/// @{

/// @brief Represents a physical unit
/// @note New units may be introduced here, whenever necessary
enum Unit
{
    native,  ///< Default (native) unit of whatever returns some value 
    linear,  ///< Value of a sample (voltage) in a linear domain
    dB   ///< Value os something in decibels. Can represent voltage, power, or a domain-specific unit like "LUFS" or "dB TP"
};

constexpr double inf_dB = hart::inf;
constexpr double oo_dB = hart::inf;
constexpr double pi_rad = hart::pi;
constexpr double twoPi_rad = hart::twoPi;
constexpr double halfPi_rad = hart::halfPi;

constexpr double operator"" _s (long double val) { return static_cast<double> (val); }
constexpr double operator"" _s (unsigned long long int val) { return static_cast<double> (val); }
constexpr double operator"" _ms (long double val) { return static_cast<double> (1e-3 * val); }
constexpr double operator"" _ms (unsigned long long int val) { return static_cast<double> (1e-3 * val); }
constexpr double operator"" _us (long double val) { return static_cast<double> (1e-6 * val); }
constexpr double operator"" _us (unsigned long long int val) { return static_cast<double> (1e-6 * val); }
constexpr double operator"" _ns (long double val) { return static_cast<double> (1e-9 * val); }
constexpr double operator"" _ns (unsigned long long int val) { return static_cast<double> (1e-9 * val); }
constexpr double operator"" _dB (long double val) { return static_cast<double> (val); }
constexpr double operator"" _dB (unsigned long long int val) { return static_cast<double> (val); }
constexpr double operator"" _dBTP (long double val) { return static_cast<double> (val); }
constexpr double operator"" _dBTP (unsigned long long int val) { return static_cast<double> (val); }
constexpr double operator"" _Hz (long double val) { return static_cast<double> (val); }
constexpr double operator"" _Hz (unsigned long long int val) { return static_cast<double> (val); }
constexpr double operator"" _kHz (long double val) { return static_cast<double> (1e3 * val); }
constexpr double operator"" _kHz (unsigned long long val) { return static_cast<double> (1e3 * val); }
constexpr double operator"" _rad (long double val) { return static_cast<double> (val); }
constexpr double operator"" _rad (unsigned long long val) { return static_cast<double>(val); }
constexpr double operator"" _deg (long double val) { return static_cast<double> (val * hart::pi / 180.0); }
constexpr double operator"" _deg (unsigned long long val) { return static_cast<double>(val) * hart::pi / 180.0; }
constexpr double operator"" _cents (long double val) { return static_cast<double> (val); }
constexpr double operator"" _cents (unsigned long long val) { return static_cast<double>(val); }

/// @}

#endif  // HART_DO_NOT_ADD_UNITS
