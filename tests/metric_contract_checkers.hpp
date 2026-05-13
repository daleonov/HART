#pragma once

#include <algorithm>  // min()
#include <string>
#include <vector>

#include "hart.hpp"

class SingleChannelMetricContractChecker
{
public:
    SingleChannelMetricContractChecker& clear()
    {
        m_label.clear();
        m_expectedCustomChannels.clear();
        m_defaultChannels.clear();
        m_expectedChannels = m_defaultChannels;
        m_expectedUnit = hart::Unit::native;
        m_expectedSlice = hart::Slice::whole();
        m_reportedNumChannels = 1;
        m_expectDefaultChannels = true;
        m_observedChannels.clear();

        return *this;
    }

    SingleChannelMetricContractChecker& withExpectedChannels (std::vector<size_t>&& channels)
    {
        m_expectDefaultChannels = false;
        m_expectedCustomChannels = std::move (channels);
        m_observedChannels.reserve (m_expectedCustomChannels.size());
        return *this;
    }

    SingleChannelMetricContractChecker& withDefaultChannels (std::vector<size_t>&& channels)
    {
        m_defaultChannels = std::move (channels);
        return *this;
    }

    SingleChannelMetricContractChecker& withExpectedUnit (hart::Unit unit)
    {
        m_expectedUnit = unit;
        return *this;
    }

    SingleChannelMetricContractChecker& withExpectedSlice (hart::Slice slice)
    {
        m_expectedSlice = slice;
        return *this;
    }

    SingleChannelMetricContractChecker& withReportedNumChannels (size_t numChannels)
    {
        m_reportedNumChannels = numChannels;
        return *this;
    }

    SingleChannelMetricContractChecker& withLabel (const std::string& label)
    {
        m_label = label;
        return *this;
    }

    hart::MetricQuery<double> operator() ()
    {
        hart::MetricQuery<double>::SingleChannelMetricEvaluator evaluator =
            [this]
            (size_t channel, hart::Slice slice, hart::Unit requestedUnit)
            -> double
        {
            HART_EXPECT_NE (
                std::find (m_expectedChannels.begin(), m_expectedChannels.end(), channel),
                m_expectedChannels.end()
                ) << m_label;

            m_observedChannels.push_back (channel);

            HART_EXPECT_EQ (slice.type, m_expectedSlice.type) << m_label;
            HART_EXPECT_FLOAT_EQ (slice.start, m_expectedSlice.start, 1e-8) << m_label;
            HART_EXPECT_FLOAT_EQ (slice.stop, m_expectedSlice.stop, 1e-8) << m_label;

            HART_EXPECT_EQ (requestedUnit, m_expectedUnit) << m_label;

            return 0.0;
        };

        if (m_defaultChannels.empty())
            m_defaultChannels = std::move (hart::ChannelSubsets::allChannels (m_reportedNumChannels));

        m_expectedChannels = m_expectDefaultChannels ? m_defaultChannels : m_expectedCustomChannels;

        return hart::MetricQuery<double> (
            std::move (evaluator),
            m_reportedNumChannels,
            std::move (m_defaultChannels)
        );
    }

    void verify() const
    {
        HART_ASSERT_EQ (m_expectedChannels.size(), m_observedChannels.size()) << m_label;

        for (size_t i = 0; i < m_observedChannels.size(); ++i)
            HART_EXPECT_EQ (m_observedChannels[i], m_expectedChannels[i]) << m_label;
    }

private:
    std::string m_label;
    std::vector<size_t> m_expectedCustomChannels;
    std::vector<size_t> m_defaultChannels;
    std::vector<size_t>& m_expectedChannels = m_defaultChannels;
    hart::Unit m_expectedUnit = hart::Unit::native;
    hart::Slice m_expectedSlice = hart::Slice::whole();
    size_t m_reportedNumChannels = 1;
    bool m_expectDefaultChannels = true;

    std::vector<size_t> m_observedChannels;
};
