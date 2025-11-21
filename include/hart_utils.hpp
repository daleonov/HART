#pragma once

#include <algorithm>  // min(), max()
#include <cctype>  // isalpha()
#include <cmath>  // pow()
#include <limits>  // infinity()
#include <memory>
#include <string>
#include <unordered_map>

#include "hart_cliconfig.hpp"

namespace hart
{

/// @defgroup Utilities Utilities
/// @brief Handy functions and constants
/// @{

/// @brief Infinity
constexpr double inf = std::numeric_limits<double>::infinity();

/// @brief Infinity
constexpr double oo = inf;

/// @brief pi
constexpr double pi = 3.14159265358979323846;

/// @brief 2 * pi
constexpr double twoPi = 2.0 * pi;

/// @brief pi / 2
constexpr double halfPi = pi / 2.0;

/// @brief `std::clamp()` replacement for C++11
template <typename NumericType>
NumericType clamp (const NumericType& value, const NumericType& low, const NumericType& high)
{
    return std::min<NumericType> (std::max<NumericType> (value, low), high);
}

/// @brief Converts dB to linear value (ratio)
/// @param valueDb Value in decibels
/// @return Value in linear domain
template <typename SampleType>
inline static SampleType decibelsToRatio (SampleType valueDb)
{
    if (valueDb < -120)
        return 0;

    return std::pow (static_cast<SampleType> (10), valueDb / static_cast<SampleType> (20));
}

/// @brief Converts linear value (ratio) to dB
/// @param valueLinear Value in linear domain
/// @return Value in decibels
template <typename SampleType>
inline static SampleType ratioToDecibels (SampleType valueLinear)
{
    if (valueLinear < 1e-6)
        return -120;

    return static_cast<SampleType> (20 * std::log10 (valueLinear));
}

/// @brief Compares two floating point numbers within a given tolerance
template <typename SampleType>
inline static SampleType floatsEqual (SampleType a, SampleType b, SampleType epsilon = (SampleType) 1e-8)
{
    return std::abs (a - b) < epsilon;
}

/// @brief Compares two floating point numbers within a given tolerance
template <typename SampleType>
inline static SampleType floatsNotEqual (SampleType a, SampleType b, SampleType epsilon = (SampleType) 1e-8)
{
    return std::abs (a - b) >= epsilon;
}

/// @brief Rounds a floating point value to a `size_t` value
template <typename SampleType>
inline static size_t roundToSizeT (SampleType x)
{
    return static_cast<size_t> (x + (SampleType) 0.5);
}

/// @brief Keeps phase in 0..twoPi range
template <typename SampleType>
SampleType wrapPhase (const SampleType phaseRadians)
{
    SampleType wrappedPhaseRadians = std::remainder (phaseRadians, (SampleType) hart::twoPi);

    if (wrappedPhaseRadians < 0.0)
        wrappedPhaseRadians += hart::twoPi;

    return wrappedPhaseRadians;
}

/// @brief Checks if the provided file path is absolute
inline static bool isAbsolutePath (const std::string& path)
{
    if (path.empty())
        return false;

    if (path[0] == '/' || path[0] == '\\')
        return true;

    #ifdef _WIN32
    if (path.size() > 1 && std::isalpha (path[0]) && path[1] == ':')
        return true;
    #endif

    return false;
}

/// @brief Converts path to absolute, if it's relative
/// @deials Relative paths are resolved based on a provided `--data-root-path` CLI argument 
inline static std::string toAbsolutePath (const std::string& path)
{
    if (isAbsolutePath(path))
        return path;

    return CLIConfig::getInstance().getDataRootPath() + '/' + path;
}

/// @brief `std::unordered_map::contains()` replacement for C++11
template <typename KeyType, typename ValueType>
inline static bool contains (const std::unordered_map<KeyType, ValueType>& map, const KeyType& key)
{
    return map.find (key) != map.end();
}

/// @brief `std::make_unique()` replacement for C++11
/// @details For C++11 compatibility only. If you're one C++14 or later, just use STL version.
template<typename ObjectType, typename... Args>
std::unique_ptr<ObjectType> make_unique (Args&&... args)
{
    return std::unique_ptr<ObjectType> (new ObjectType (std::forward<Args> (args)...));
}

/// @brief Defines a basic string representation of your class
/// @details If your class takes ctor arguments, it's strongly encouraged to make a proper
/// implementation of represent(), so that you get more detailed test failure reports.
/// See @ref DSP::represent(), @ref Macthers::represent(), @ref Signal::represent() for the description.
#define HART_DEFINE_GENERIC_REPRESENT(ClassName) \
    virtual void represent(std::ostream& stream) const override \
    { \
        stream << #ClassName "()"; \
    }

/// @}

}  // namespace hart
