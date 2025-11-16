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

constexpr double inf = std::numeric_limits<double>::infinity();
constexpr double oo = inf;
constexpr double pi = 3.14159265358979323846;
constexpr double twoPi = 2.0 * pi;
constexpr double halfPi = pi / 2.0;

template <typename NumericType>
NumericType clamp (const NumericType& value, const NumericType& low, const NumericType& high)
{
    return std::min<NumericType> (std::max<NumericType> (value, low), high);
}

template <typename SampleType>
inline static SampleType decibelsToRatio (SampleType valueDb)
{
    if (valueDb < -120)
        return 0;

    return std::pow (static_cast<SampleType> (10), valueDb / static_cast<SampleType> (20));
}

template <typename SampleType>
inline static SampleType ratioToDecibels (SampleType valueLinear)
{
    if (valueLinear < 1e-6)
        return -120;

    return static_cast<SampleType> (20 * std::log10 (valueLinear));
}

template <typename SampleType>
inline static SampleType floatsEqual (SampleType a, SampleType b, SampleType epsilon = (SampleType) 1e-8)
{
    return std::abs (a - b) < epsilon;
}

template <typename SampleType>
inline static SampleType floatsNotEqual (SampleType a, SampleType b, SampleType epsilon = (SampleType) 1e-8)
{
    return std::abs (a - b) >= epsilon;
}

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

inline static std::string toAbsolutePath (const std::string& path)
{
    if (isAbsolutePath(path))
        return path;

    return CLIConfig::get().getDataRootPath() + '/' + path;
}

template <typename KeyType, typename ValueType>
inline static bool contains (const std::unordered_map<KeyType, ValueType>& map, const KeyType& key)
{
    return map.find (key) != map.end();
}

// For C++11 compatibility
// If you're one C++14 or later, just use STL version
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

}  // namespace hart
