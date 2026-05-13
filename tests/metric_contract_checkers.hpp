#pragma once

#include <algorithm>  // min()
#include <string>
#include <utility>  // pair, make_pair()
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

class ChannelPairMetricContractChecker
{
public:
    ChannelPairMetricContractChecker& clear()
    {
        m_label.clear();
        m_expectedCustomChannelPairs.clear();
        m_defaultChannelPairs.clear();
        m_expectedChannelPairs = m_defaultChannelPairs;
        m_expectedUnit = hart::Unit::native;
        m_expectedSlice = hart::Slice::whole();
        m_reportedNumChannelsA = 1;
        m_reportedNumChannelsB = 1;
        m_expectDefaultChannelPairs = true;
        m_observedChannelPairs.clear();

        return *this;
    }

    ChannelPairMetricContractChecker& withExpectedChannelPairs (std::vector<std::pair<size_t, size_t>>&& channelPairs)
    {
        m_expectDefaultChannelPairs = false;
        m_expectedCustomChannelPairs = std::move (channelPairs);
        m_observedChannelPairs.reserve (m_expectedCustomChannelPairs.size());
        return *this;
    }

    ChannelPairMetricContractChecker& withDefaultChannelPairs (std::vector<std::pair<size_t, size_t>>&& channelPairs)
    {
        m_defaultChannelPairs = std::move (channelPairs);
        return *this;
    }

    ChannelPairMetricContractChecker& withExpectedUnit (hart::Unit unit)
    {
        m_expectedUnit = unit;
        return *this;
    }

    ChannelPairMetricContractChecker& withExpectedSlice (hart::Slice slice)
    {
        m_expectedSlice = slice;
        return *this;
    }

    ChannelPairMetricContractChecker& withReportedNumChannels (size_t numChannelsA, size_t numChannelsB)
    {
        m_reportedNumChannelsA = numChannelsA;
        m_reportedNumChannelsB = numChannelsB;
        return *this;
    }

    ChannelPairMetricContractChecker& withLabel (const std::string& label)
    {
        m_label = label;
        return *this;
    }

    hart::MetricQuery<double> operator() ()
    {
        hart::MetricQuery<double>::ChannelPairMetricEvaluator evaluator =
            [this]
            (size_t channelA, size_t channelB, hart::Slice slice, hart::Unit requestedUnit)
            -> double
        {
            const auto expectedChannelPair = std::make_pair (channelA, channelB);

            HART_EXPECT_NE (
                std::find (m_expectedChannelPairs.begin(), m_expectedChannelPairs.end(), expectedChannelPair),
                m_expectedChannelPairs.end()
                ) << m_label;

            m_observedChannelPairs.push_back (expectedChannelPair);

            HART_EXPECT_EQ (slice.type, m_expectedSlice.type) << m_label;
            HART_EXPECT_FLOAT_EQ (slice.start, m_expectedSlice.start, 1e-8) << m_label;
            HART_EXPECT_FLOAT_EQ (slice.stop, m_expectedSlice.stop, 1e-8) << m_label;

            HART_EXPECT_EQ (requestedUnit, m_expectedUnit) << m_label;

            return 0.0;
        };

        if (m_defaultChannelPairs.empty())
        {
            const size_t numChannels = std::min (m_reportedNumChannelsA, m_reportedNumChannelsB);
            m_defaultChannelPairs = hart::ChannelSubsets::diagonalChannelPairs (numChannels);
        }

        m_expectedChannelPairs = m_expectDefaultChannelPairs ? m_defaultChannelPairs : m_expectedCustomChannelPairs;

        return hart::MetricQuery<double> (
            std::move (evaluator),
            m_reportedNumChannelsA,
            m_reportedNumChannelsB,
            std::move (m_defaultChannelPairs)
        );
    }

    void verify() const
    {
        HART_ASSERT_EQ (m_expectedChannelPairs.size(), m_observedChannelPairs.size()) << m_label;

        for (size_t i = 0; i < m_observedChannelPairs.size(); ++i)
        {
            HART_EXPECT_EQ (m_observedChannelPairs[i].first, m_expectedChannelPairs[i].first) << m_label;
            HART_EXPECT_EQ (m_observedChannelPairs[i].second, m_expectedChannelPairs[i].second) << m_label;
        }
    }

private:
    std::string m_label;
    std::vector<std::pair<size_t, size_t>> m_expectedCustomChannelPairs;
    std::vector<std::pair<size_t, size_t>> m_defaultChannelPairs;
    std::vector<std::pair<size_t, size_t>>& m_expectedChannelPairs = m_defaultChannelPairs;
    hart::Unit m_expectedUnit = hart::Unit::native;
    hart::Slice m_expectedSlice = hart::Slice::whole();
    size_t m_reportedNumChannelsA = 1;
    size_t m_reportedNumChannelsB = 1;
    bool m_expectDefaultChannelPairs = true;

    std::vector<std::pair<size_t, size_t>> m_observedChannelPairs;
};
