#pragma once

#include <cmath>  // abs()
#include <iomanip>
#include <sstream>

#include "hart_cliconfig.hpp"
#include "matchers/hart_matcher.hpp"
#include "hart_utils.hpp"  // decibelsToRatio()

namespace hart
{

/// @brief Checks whether the audio peaks below specific level
/// @details This matcher will calculate peak value on a block-by-block basis.
/// It checks where all selected channels peak collectively, not for each individual channel.
/// To do more strict per channel checks, use multiple matchers with @ref atChannel(), so that
/// each matcher only targets one specific channel. 
/// @note Tip: To check if the audio peaks @em above some level, just flip your assertion, e.g.
/// @code expectFalse (PeaksBelow (-3_Db)) @endcode or @code assertFalse (PeaksBelow (-3_Db)) @endcode
/// @attention It checks the sample peaks, not the inter-sample peaks (a popular metric in audio
/// mastering community). For inter-sample peak checking, see @ref `TruePeaksBelow`.
template<typename SampleType>
class PeaksBelow:
    public Matcher<SampleType, PeaksBelow<SampleType>>
{
public:
    /// @brief Creates a matcher for a specific peak level
    /// @param thresholdDb Expected sample peak threshold in decibels
    /// @param toleranceLinear Absolute tolerance for comparing frames, in linear domain (not decibels)
    PeaksBelow (double thresholdDb, double toleranceLinear = 1e-3):
        m_thresholdDb ((SampleType) thresholdDb),
        m_thresholdLinear (static_cast<SampleType> (decibelsToRatio (thresholdDb) + toleranceLinear)),
        m_toleranceLinear (toleranceLinear)
    {
    }

    void prepare (double /*sampleRateHz*/, size_t /* numChannels */, size_t /* maxBlockSizeFrames */) override {}

    bool match (const AudioBuffer<SampleType>& /* inputAudio */, const AudioBuffer<SampleType>& observedOutputAudio) override
    {
        for (size_t channel = 0; channel < observedOutputAudio.getNumChannels(); ++channel)
        {
            if (! this->appliesToChannel (channel))
                continue;

            for (size_t frame = 0; frame < observedOutputAudio.getNumFrames(); ++frame)
            {
                const SampleType observedPeakLinear = std::abs (observedOutputAudio[channel][frame]);

                if (observedPeakLinear > m_thresholdLinear)
                {
                    m_failedFrame = frame;
                    m_failedChannel = channel;
                    m_observedPeakDb = ratioToDecibels (observedPeakLinear);
                    return false;
                }
            }
        }

        return true;
    }

    bool canOperatePerBlock() const override
    {
        return true;
    }

    virtual MatcherFailureDetails getFailureDetails() const override
    {
        std::stringstream stream;
        stream << "Observed audio peaks at at least "
            << dbPrecision << m_observedPeakDb << " dB";

        MatcherFailureDetails details;
        details.frame = m_failedFrame;
        details.channel = m_failedChannel;
        details.description = stream.str();
        return details;
    }

    void represent (std::ostream& stream) const override
    {
        stream << "PeaksBelow ("
            << dbPrecision << m_thresholdDb << "_dB, "
            << linPrecision << m_toleranceLinear << ')';
    }

private:
    const SampleType m_thresholdDb;
    const SampleType m_thresholdLinear;
    const SampleType m_toleranceLinear;

    size_t m_failedFrame = 0;
    size_t m_failedChannel = 0;
    SampleType m_observedPeakDb = (SampleType) 0;
};

HART_MATCHER_DECLARE_ALIASES_FOR (PeaksBelow)

}  // namespace hart
