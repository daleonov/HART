#pragma once

#include <iomanip>
#include <ostream>
#include "hart_cliconfig.hpp"

namespace hart {

/// @defgroup PrecisionManipulators Precision Manipulators
/// @brief Stream manipulators to set decimal precision for unit-specific output
/// @details Precision values come from CLI config (e.g. `--db-decimals`).
/// Use like: `stream << hart::dbPrecision << myValue;`
/// @{

/// @brief Sets number of decimal places for linear (sample) values
/// @details The precision is set via `--lin-decimals` CLI argument
/// @see CLIConfig::getLinDecimals()
inline std::ostream& linPrecision (std::ostream& stream)
{
    return stream << std::fixed << std::setprecision (CLIConfig::get().getLinDecimals());
}

/// @brief Sets number of decimal places for values in decibels
/// @details The precision is set via `--db-decimals` CLI argument
/// @see CLIConfig::getDbDecimals()
inline std::ostream& dbPrecision (std::ostream& stream)
{
    return stream << std::fixed << std::setprecision (CLIConfig::get().getDbDecimals());
}

/// @brief Sets number of decimal places for values in seconds
/// @details The precision is set via `--sec-decimals` CLI argument
/// @see CLIConfig::getSecDecimals()
inline std::ostream& secPrecision (std::ostream& stream)
{
    return stream << std::fixed << std::setprecision (CLIConfig::get().getSecDecimals());
}

/// @brief Sets number of decimal places for values in hertz
/// @details The precision is set via `--hz-decimals` CLI argument
/// @see CLIConfig::getHzDecimals()
inline std::ostream& hzPrecision (std::ostream& stream)
{
    return stream << std::fixed << std::setprecision (CLIConfig::get().getHzDecimals());
}

/// @brief Sets number of decimal places for values in radians
/// @details The precision is set via `--rad-decimals` CLI argument
/// @see CLIConfig::getRadDecimals()
inline std::ostream& radPrecision (std::ostream& stream)
{
    return stream << std::fixed << std::setprecision (CLIConfig::get().getRadDecimals());
}

/// @}

}  // namespace hart