#pragma once

#include <sstream>
#include <string>

namespace hart {

/// @brief Helper class for building strings with string stream syntax
class Str
{
public:
	/// @brief Appends value to a string stream
	/// @param value Any printable value
    template<typename ValueType>
    Str& operator<< (const ValueType& value)
    {
        m_stringStream << value;
        return *this;
    }

    /// @brief Returns a std::string constructed from a stream
    std::string toStdString() const
    {
    	return m_stringStream.str();
    }

private:
    std::ostringstream m_stringStream;
};

/// @brief A helper to construct strings using the "<<" syntax
/// @details Usage: `HART_STR ("Some text: " << someValue << "...")`
#define HART_STR(...) (hart::Str() << __VA_ARGS__).toStdString()

}  // namespace hart
