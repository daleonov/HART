#pragma once

#include <cstdint>  // uintptr_t
#include <iomanip>
#include <ostream>
#include <sstream>
#include <string>
#include <type_traits>
#include <utility>

namespace hart
{

/// @brief A helper to test whether an object can be printed via the "<<" operator
/// @private
template <typename Type>
class IsStreamInsertable
{
private:
    template <typename CandidateType>
    static auto test (int) -> decltype (
        std::declval<std::ostream&>() << std::declval<const CandidateType&>(),
        std::true_type()
        );

    template <typename>
    static std::false_type test (...);

public:
    static const bool value = decltype (test<Type> (42))::value;
};

/// @private
template <typename Type>
std::string pointerToString (const Type* value)
{
    if (value == nullptr)
        return "nullptr";

    std::ostringstream stream;
    stream << "<pointer to 0x" << std::hex << reinterpret_cast<std::uintptr_t> (value) << '>';
    return stream.str();
}

/// @private
template <typename Type>
std::string objectAddressToString (const Type& value)
{
    std::ostringstream stream;
    stream << "<object at 0x" << std::hex << reinterpret_cast<std::uintptr_t> (&value) << '>';
    return stream.str();
}

/// @private
inline std::string toString (const std::string& value)
{
    return '"' + value + '"';
}

/// @private
inline std::string toString (const char* value)
{
    if (value == nullptr)
        return "nullptr";

    return '"' + std::string (value) + '"';
}

/// @private
inline std::string toString (bool value)
{
    return value == true ? "true" : "false";
}

/// @brief Creates a string representation of a raw pointer
/// @private
template <typename Type>
std::string toString (Type* value)
{
    return pointerToString (value);
}

/// @brief Creates a string representation of an object that supports printing via the "<<" operator
/// @private
template <typename Type>
typename std::enable_if<IsStreamInsertable<Type>::value, std::string>::type
toString (const Type& value)
{
    std::ostringstream stream;
    stream << value;
    return stream.str();
}

/// @brief Creates a fallback string representation of an object that doesn't support printing via the "<<" operator
/// @private
template <typename Type>
typename std::enable_if<! IsStreamInsertable<Type>::value, std::string>::type
toString (const Type& value)
{
    return objectAddressToString (value);
}

}  // namespace hart
