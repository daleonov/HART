#pragma once

#include <algorithm>  // min(), max()
#include <cctype>  // isalpha()
#include <cmath>  // pow()
#include <limits>  // infinity()
#include <string>

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

    return std::pow ((SampleType) 10, valueDb / ((SampleType) 20));
}

template <typename SampleType>
inline static SampleType ratioToDecibels (SampleType valueLinear)
{
    if (valueLinear < 1e-6)
        return -120;

    return std::pow ((SampleType) 10, valueLinear / ((SampleType) 20));
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

}  // namespace hart
