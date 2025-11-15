#pragma once

#include <cmath>  // abs()

#include "matchers/hart_matcher.hpp"
#include "hart_utils.hpp"  // decibelsToRatio()

namespace hart
{

template<typename SampleType>
class PeaksBelow:
    public Matcher<SampleType>
{
public:
    PeaksBelow (SampleType tresholdDb):
        m_tresholdDb (tresholdDb),
        m_tresholdLinear (decibelsToRatio (tresholdDb))
    {
    }

    void prepare (double /*sampleRateHz*/, size_t /* numOutputChannels */, size_t /* maxBlockSizeFrames */) override {}

    bool match (const AudioBuffer<SampleType>& observedAudio) override
    {
        for (size_t channel = 0; channel < observedAudio.getNumChannels(); ++channel)
            for (size_t frame = 0; frame < observedAudio.getNumFrames(); ++frame)
                if (std::abs (observedAudio[channel][frame]) > m_tresholdLinear)
                    return false;

        return true;
    }

    bool canOperatePerBlock() override
    {
        return true;
    }

    void reset() override {}

    virtual std::unique_ptr<Matcher<SampleType>> copy() const override
    {
        return std::make_unique<PeaksBelow<SampleType>> (*this);
    }

    std::string describe() const override
    {
        return std::string ("Peaks Below: ") + std::to_string (m_tresholdDb) + "dB";
    }

private:
    const SampleType m_tresholdDb;
    const SampleType m_tresholdLinear;
};

}  // namespace hart
