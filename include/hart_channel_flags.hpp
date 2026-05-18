#pragma once

#include <bitset>
#include <vector>

#include "hart_exceptions.hpp"

namespace hart
{

/// @brief A set of boolean flags mapped to each audio channel
/// @ingroup DataStructures
class ChannelFlags
{
private:
    static constexpr size_t m_maxChannels = 64;  // Can be expanded if you need more

public:
    /// @brief Creates a new channel flags object
    /// @param defaultValues Initial value for all flags
    /// @param numChannels Size of the contailer
    ChannelFlags (bool defaultValues = true, size_t numChannels = m_maxChannels)
    {
        if (numChannels > m_maxChannels)
            HART_THROW_OR_RETURN_VOID (hart::SizeError, "Number of channels exceeds maximum possible amount");

        m_numChannels = numChannels;
        setAllTo (defaultValues);
    }

    /// @brief Sets all flags to a new value
    /// @param newValues New values for all flags
    void setAllTo (bool newValues)
    {
        if (newValues == true)
            m_flags.set();
        else
            m_flags.reset();
    }

    /// @brief Returns the size (not capacity) of the container
    /// @details This size is guaranteed to be equal to the number
    /// of channels in whatever it's assotiated with, or more.
    size_t size() const noexcept
    {
        return m_numChannels;
    }

    /// @brief Resizes the container
    /// @details Does not change the capacity - it's fixed.
    /// Does not change the flags values.
    void resize (size_t newNumChannels)
    {
        if (newNumChannels > m_maxChannels)
            HART_THROW_OR_RETURN_VOID (hart::SizeError, "Number of channels exceeds maximum possible amount");

        m_numChannels = newNumChannels;
    }

    /// @brief Access the flag value for a specific channel
    /// @param channel Number of channel (0-based)
    /// @return A reference to underlying storage for the corresponding flag
    std::bitset<m_maxChannels>::reference operator[] (size_t channel) {
        if (channel >= m_numChannels)
            HART_THROW_OR_RETURN (hart::SizeError, "ChannelFlags index is out of range", {});

        return m_flags[channel];
    }

    /// @brief Access the flag value for a specific channel
    /// @param channel Number of channel (0-based)
    /// @return Value of a corresponding flag
    bool operator[] (size_t channel) const {
        if (channel >= m_numChannels)
            HART_THROW_OR_RETURN (hart::SizeError, "ChannelFlags index is out of range", false);

        return m_flags.test (channel);
    }

    /// @brief Checks if all flags are set to `true`
    /// @return `true` if all flags for all channels are `true`, `false` otherwise
    bool allTrue() const noexcept
    {
        // TODO: Can it be O(1)?

        for (size_t i = 0; i < m_numChannels; ++i)
            if (m_flags.test (i) == false)
                return false;

        return true;
    }

    /// @brief Checks how many channels are marked with `true`
    /// @return Amount of flags set to `true`
    size_t numTrue() const noexcept
    {
        size_t res = 0;

        for (size_t channel = 0; channel < m_numChannels; ++channel)
            res += m_flags.test (channel);

        return res;
    }

    /// @brief Checks if any of the flags is set to `true`
    /// @return `true` if flag for at least one channel is `true`, `false` otherwise
    bool anyTrue() const noexcept
    {
        // TODO: Can it be O(1)?

        for (size_t i = 0; i < m_numChannels; ++i)
            if (m_flags.test (i) == true)
                return true;

        return false;
    }

    /// @brief Makes text representation of itself as a initializer list of active channels
    /// @param[out] stream Output stream to write to
    void representAsInitializerList (std::ostream& stream) const
    {
        stream << '{';
        bool notFirst = false;

        for (size_t i = 0; i < m_numChannels; ++i)
        {
            if (m_flags.test (i) == false)
                continue;

            if (notFirst)
                stream << ", ";

            stream << i;
            notFirst = true;
        }

        stream << '}';
    }

private:
    size_t m_numChannels = m_maxChannels;
    std::bitset<m_maxChannels> m_flags;
};

}  //  namespace hart
