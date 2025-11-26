#pragma once

#include <bitset>
#include "hart_exceptions.hpp"

namespace hart
{

class ChannelFlags
{
private:
    static constexpr size_t m_maxChannels = 64;  // Can be expanded if you need more

public:
    ChannelFlags (bool defaultValues = true, size_t numChannels = m_maxChannels)
    {
        if (numChannels > m_maxChannels)
            HART_THROW_OR_RETURN_VOID (hart::SizeError, "Number of channels exceeds maximum possible amount");

        m_numChannels = m_maxChannels;
        setAllTo (defaultValues);
    }

    void setAllTo (bool newValues)
    {
        if (newValues == true)
            m_flags.set();
        else
            m_flags.reset();
    }

    size_t size() const noexcept
    {
        return m_numChannels;
    }

    void resize (size_t newNumChannels)
    {
        if (newNumChannels > m_maxChannels)
            HART_THROW_OR_RETURN_VOID (hart::SizeError, "Number of channels exceeds maximum possible amount");

        m_numChannels = newNumChannels;
    }

    std::bitset<m_maxChannels>::reference operator[] (size_t channel) {
        if (channel >= m_numChannels)
            HART_THROW_OR_RETURN (hart::SizeError, "ChannelFlags index is out of range", {});

        return m_flags[channel];
    }

    bool operator[] (size_t channel) const {
        if (channel >= m_numChannels)
            HART_THROW_OR_RETURN (hart::SizeError, "ChannelFlags index is out of range", false);

        return m_flags.test (channel);
    }

    bool allTrue() const noexcept
    {
        // TODO: Can it be O(1)?

        for (size_t i = 0; i < m_maxChannels; ++i)
            if (m_flags.test (i) == false)
                return false;

        return true;
    }

    bool anyTrue() const noexcept
    {
        // TODO: Can it be O(1)?

        for (size_t i = 0; i < m_maxChannels; ++i)
            if (m_flags.test (i) == true)
                return true;

        return false;
    }

private:
    size_t m_numChannels = m_maxChannels;
    std::bitset<m_maxChannels> m_flags;
};

}  //  namespace hart
