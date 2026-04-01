#pragma once

namespace hart
{

/// @brief Defines how silence in various algorithms
/// @details Exact meaning of each option depends on the algorithm,
/// so refer to the docs of each specific algorithm or class for details. 
/// @ingroup Utilities
enum class SilencePolicy
{
    strict,
    relaxed
};

}  // namespace hart
