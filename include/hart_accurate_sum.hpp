#pragma once

namespace hart
{

///@brief Implements Kahan algorithm for floating point accumulations
///@tparam SampleType Type of values to accumulate, typically `float` or `double`
///@ingroup Utilities
template<typename SampleType>
class AccurateSum
{
public:
    ///@brief Inits AccurateSum with a specific value
    ///@param initialSum Initial sum value
    AccurateSum (SampleType initialSum):
        m_sum (initialSum),
        m_compensation ((SampleType) 0)
    {
    }

    ///@brief Assigns AccurateSum's accumulated sum to a specific value
    ///@param initialSum Initial sum value
    AccurateSum& operator= (SampleType initialSum)
    {
        m_sum = initialSum;
        m_compensation = (SampleType) 0;
        return *this;
    }

    /// @brief Adds a value to a sum, tracking the potential floating point error
    /// @param value Value to add to the sum
    /// @return Reference to updated AccurateSum
    AccurateSum& operator+= (SampleType value)
    {
        const SampleType corrected = value - m_compensation;
        const SampleType newSum = m_sum + corrected;
        m_compensation = (newSum - m_sum) - corrected;
        m_sum = newSum;
        return *this;
    }

    /// @brief Value accumulated by the += operator
    operator SampleType() const
    {
        return m_sum;
    }

    /// @brief Value accumulated by the += operator, converted to the requested type
    template<typename RequestedType>
    RequestedType get() const
    {
        return static_cast<RequestedType> (m_sum);
    }

private:
    SampleType m_sum = (SampleType) 0;
    SampleType m_compensation = (SampleType) 0;
};

}  // namespace hart
