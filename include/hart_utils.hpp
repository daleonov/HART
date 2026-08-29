#pragma once

#include <algorithm>  // min(), max()
#include <cctype>  // isalpha()
#include <cmath>  // pow()
#include <exception>
#include <fstream>
#include <limits>  // infinity(), nan()
#include <memory>
#include <ostream>
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

/// @brief Helper values for channel indices
enum Channel
{
    left = 0,
    right = 1
};

/// @brief Helper values for mid-side channel indices
enum MidSideChannel
{
    mid = 0,
    side = 1
};

/// @brief Helper values for something that could loop, like a Signal
enum class Loop
{
    no,
    yes
};

/// @brief Helper values for something that could normalise something
enum class Normalise
{
    no,
    yes
};

/// @brief Oversampling ratio
enum Oversampling
{
    x4 = 4,
    x8 = 8,
    x16 = 16
};

/// @brief  @brief Interpolation method
enum class Interpolation
{
    nearest,
    linear
};

inline std::ostream& operator<< (std::ostream& os, Oversampling oversampling)
{
    return os << "Oversampling::x" << static_cast<int> (oversampling);
}

/// @brief Returns a quiet NaN value for the given floating-point type.
template<typename FloatType>
inline FloatType nan()
{
    return std::numeric_limits<FloatType>::quiet_NaN();
}

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

/// @brief Converts dB to linear value (power)
/// @param valueDb Value in decibels
/// @return Value in linear domain
template <typename SampleType>
inline static SampleType decibelsToPower (SampleType valueDb)
{
    if (valueDb < -120)
        return 0;

    return std::pow (static_cast<SampleType> (10), valueDb / static_cast<SampleType> (10));
}

/// @brief Converts linear value (power) to dB
/// @param valueLinear Value in linear domain
/// @return Value in decibels
template <typename SampleType>
inline static SampleType powerToDecibels (SampleType valueLinear)
{
    if (valueLinear < 1e-12)
        return -120;

    return static_cast<SampleType> (10 * std::log10 (valueLinear));
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

/// @brief Converts frequency difference in cents to frequence ratio
inline double centsToRatio (double cents)
{
    return std::pow (2.0, cents / 1200.0);
}

/// @brief Adds an offset in cents to a frequency in Hz
inline double addCents (double baseFrequencyHz, double cents)
{
    return baseFrequencyHz * centsToRatio (cents);
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

/// @brief Finds next power of 2 after a non-negative number x
static size_t nextPowerOfTwo (size_t x)
{
    size_t power = 1;

    while (power < x)
        power <<= 1;

    return power;
}

/// @brief Finds previous power of 2 after a non-negative number x
/// @note If x is a power of 2 itself, it will return x
static size_t previousPowerOfTwo (size_t x)
{
    if (x == 0)
        return 0;

    size_t power = 1;

    while ((power << 1) < x)
        power <<= 1;

    return power;
}

// @brief Checks if number is a power of 2
static bool isPowerOfTwo (size_t x)
{
    return (x != 0) && ((x & (x - 1)) == 0);
}

// @brief Check if file exists and whether it's possible to read it
// @note There are better ways to do it post-C++17, but HART is C++11.
inline static bool fileExistsAndReadable (const std::string& path)
{
    std::ifstream file (path.c_str());
    return file.good();
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
/// @details Relative paths are resolved based on a provided `--data-root-path` CLI argument 
inline static std::string toAbsolutePath (const std::string& path)
{
    if (isAbsolutePath(path))
        return path;

    return CLIConfig::getInstance().getDataRootPath() + '/' + path;
}

/// @brief Deterministically produces a sequence of well-dispersed random seeds from a single base seed
/// @param numSeeds Number of random seeds to produce
/// @param baseRandomSeed Initial seed, from which a sequence of new seed values is derived
/// @return Vector containing `numSeeds` different random seeds
/// @note Identical `baseRandomSeed` values are guaranteed to yield identical derived seed sequences
template <typename ValueType>
std::vector<ValueType> makeRandomSeeds (size_t numSeeds, uint_fast32_t baseRandomSeed = CLIConfig::getInstance().getRandomSeed())
{
    std::seed_seq seedGenerator { baseRandomSeed };
    std::vector<ValueType> randomSeeds (numSeeds);

    // std::seed_seq produces uint32_t values, which will be implicitly cast
    // to ValueType here. It's either that, or we'll have to allocate an extra
    // temporary vector for native values, which we're not doing that.
    seedGenerator.generate (randomSeeds.begin(), randomSeeds.end());

    return randomSeeds;
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

/// @brief Returns `true` if an exception is currently being unwound
inline static bool isExceptionUnwinding()
{
#if defined(__cpp_lib_uncaught_exceptions)
    return std::uncaught_exceptions() > 0;
#else
    return std::uncaught_exception();
#endif
}

/// @brief Defines a basic string representation of your class
/// @details If your class takes ctor arguments, it's strongly encouraged to make a proper
/// implementation of `represent()`, so that you get more detailed test failure reports.
/// See @ref hart::DSP::represent(), @ref hart::Matcher::represent(),
/// @ref hart::Signal::represent() for the description.
#define HART_DEFINE_GENERIC_REPRESENT(ClassName) \
    virtual void represent(std::ostream& stream) const override \
    { \
        stream << #ClassName "()"; \
    }

/// @private
#if defined(__GNUC__) || defined(__clang__)
    #define HART_DEPRECATED(msg) __attribute__((deprecated(msg)))
#elif defined(_MSC_VER)
    #define HART_DEPRECATED(msg) __declspec(deprecated(msg))
#else
    #define HART_DEPRECATED(msg)
#endif

}  // namespace hart
