#pragma once

#include <cmath>  // abs()

#include "matchers/hart_matcher.hpp"
#include "hart_utils.hpp"  // decibelsToRatio()

namespace hart
{

/// @brief Checks whether the audio peaks below specific level
/// @note Tip: To check if the audio peaks @em above some level, just flip your assertion, e.g.
/// @code expectFalse (PeaksBelow (-3_Db)) @endcode or @code assertFalse (PeaksBelow (-3_Db)) @endcode
/// @attention It checks the sample peaks, not the inter-sample peaks (a popular metric in audio
/// mastering community). Fon inter-sample peak checking, you can make your own custom Matcher..
/// @ingroup Matchers
template<typename SampleType>
class PeaksBelow:
    public Matcher<SampleType>
{
public:
    /// @brief Creates a matcher for a specific peak level
    /// @param thresholdDb Expected sample peak threshold in decibels
    /// @param toleranceLinear Absolute tolerance for comparing frames, in linear domain (not decibels)
    PeaksBelow (SampleType thresholdDb, SampleType toleranceLinear = 1e-3):
        m_thresholdDb (thresholdDb),
        m_thresholdLinear (decibelsToRatio (thresholdDb) + m_toleranceLinear)
    {
    }

    void prepare (double /*sampleRateHz*/, size_t /* numChannels */, size_t /* maxBlockSizeFrames */) override {}

    bool match (const AudioBuffer<SampleType>& observedAudio) override
    {
        for (size_t channel = 0; channel < observedAudio.getNumChannels(); ++channel)
            for (size_t frame = 0; frame < observedAudio.getNumFrames(); ++frame)
                if (std::abs (observedAudio[channel][frame]) > m_thresholdLinear)
                    return false;

        return true;
    }

    bool canOperatePerBlock() override
    {
        return true;
    }

    void reset() override {}

    std::string describe() const override
    {
        return std::string ("Peaks Below: ") + std::to_string (m_thresholdDb) + "dB";
    }

    HART_MATCHER_DEFINE_COPY_AND_MOVE (PeaksBelow);

private:
    const SampleType m_thresholdDb;
    const SampleType m_thresholdLinear;
};

}  // namespace hart
