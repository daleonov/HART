#pragma once

namespace hart
{

/// @brief Describes whether to call reset() and/or prepare() before rendering through DSP or a Signal
enum class Preparation
{
    none,
    reset,
    prepare,
    resetAndPrepare
};

}  // namespace hart
