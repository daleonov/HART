#pragma once

#include <algorithm>  // max()
#include <cmath>  // abs()

#include "matchers/hart_matcher.hpp"
#include "hart_utils.hpp"  // decibelsToRatio()

namespace hart
{

/// @brief Checks whether the audio peaks at specific level
/// @details This matcher will calculate peak value of a full audio signal (not a per-block peaks)
/// and compare against a value provided during instantiation with a provided tolerance.
/// @attention It checks the sample peaks, not the inter-sample peaks (a popular metric in audio
/// mastering community). Fon inter-sample peak checking, you can make your own custom Matcher.
/// @ingroup Matchers
template<typename SampleType>
class PeaksAt:
    public Matcher<SampleType>
{
public:
    /// @brief Creates a matcher for a specific peak level
    /// @param targetDb Expected sample peak value in decibels
    /// @param toleranceLinear Absolute tolerance for comparing frames, in linear domain (not decibels)
    PeaksAt (double targetDb, double toleranceLinear = 1e-3):
        m_targetDb ((SampleType) targetDb),
        m_targetLinear ((SampleType) decibelsToRatio (targetDb)),
        m_toleranceLinear ((SampleType) toleranceLinear)
    {
    }

    void prepare (double /*sampleRateHz*/, size_t /* numChannels */, size_t /* maxBlockSizeFrames */) override {}

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

    std::string describe() const override
    {
        return {};
    }

    void represent (std::ostream& stream) const
    {
        stream << "PeaksAt(" << m_targetDb << ", " << m_toleranceLinear << ')';
    }

    HART_MATCHER_DEFINE_COPY_AND_MOVE (PeaksAt);

private:
    const SampleType m_targetDb;
    const SampleType m_targetLinear;
    const SampleType m_toleranceLinear;
};

}  // namespace hart
