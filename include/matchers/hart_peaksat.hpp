#pragma once

#include <algorithm>  // max()
#include <cmath>  // abs()

#include "matchers/hart_matcher.hpp"
#include "hart_utils.hpp"  // decibelsToRatio()

namespace hart
{

template<typename SampleType>
class PeaksAt:
    public Matcher<SampleType>
{
public:
    PeaksAt (SampleType targetDb, SampleType toleranceLinear = 1e-3):
        m_targetDb (targetDb),
        m_targetLinear (decibelsToRatio (targetDb)),
        m_toleranceLinear (toleranceLinear)
    {
    }

    void prepare (double /*sampleRateHz*/, size_t /* numOutputChannels */, size_t /* maxBlockSizeFrames */) override {}

    bool match (const AudioBuffer<SampleType>& observedAudio) override
    {
        SampleType observedPeakLinear = 0;

        for (size_t channel = 0; channel < observedAudio.getNumChannels(); ++channel)
            for (size_t frame = 0; frame < observedAudio.getNumFrames(); ++frame)
                observedPeakLinear = std::max (observedPeakLinear, std::abs (observedAudio[channel][frame]));

        return std::abs (observedPeakLinear - m_targetLinear) < m_toleranceLinear;
    }

    bool canOperatePerBlock() override
    {
        return false;
    }

    void reset() override {}

    virtual std::unique_ptr<Matcher<SampleType>> copy() const override
    {
        return std::make_unique<PeaksAt<SampleType>> (*this);
    }

    std::string describe() const override
    {
        return std::string ("PeaksAt (") + std::to_string (m_targetDb) + ")";
    }

private:
    const SampleType m_targetDb;
    const SampleType m_targetLinear;
    const SampleType m_toleranceLinear;
};

}  // namespace hart
