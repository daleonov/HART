#pragma once

#include <cmath>  // isnan()
#include <sstream>

#include "matchers/hart_matcher.hpp"

namespace hart
{

/// @brief Checks whether the audio has no `NaN` values
/// @details This matcher fails it at least one of the frames in the output audio contains `NaN` value
/// @ingroup Matchers
template<typename SampleType>
class NoNaNs:
    public Matcher<SampleType, NoNaNs<SampleType>>
{
public:
    bool match (AnalysisContext<SampleType> context) override
    {
        const AudioBuffer<SampleType>& observedOutputAudio = context.outputAudio();

        for (size_t channel = 0; channel < observedOutputAudio.getNumChannels(); ++channel)
        {
            if (! this->appliesToChannel (channel))
                continue;

            for (size_t frame = 0; frame < observedOutputAudio.getNumFrames(); ++frame)
            {
                const SampleType sample = observedOutputAudio[channel][frame];

                if (std::isnan (sample))
                {
                    m_failedFrame = frame;
                    m_failedChannel = channel;
                    return false;
                }
            }
        }

        return true;
    }

    MatcherFailureDetails getFailureDetails() const override
    {
        // Potentially, we may also want display the payload in the NaN,
        // but since there's no obvious valid use case for that, we just
        // report the mere fact of NaN being found. 

        MatcherFailureDetails details;
        details.frame = m_failedFrame;
        details.channel = m_failedChannel;
        details.description = "NaN value found";

        return details;
    }

    bool canOperatePerBlock() const override
    {
        return true;
    }

    void prepare (double /*sampleRateHz*/, size_t /* numInputChannels */, size_t /* numOutputChannels */, size_t /* maxBlockSizeFrames */) override {}

    HART_DEFINE_GENERIC_REPRESENT (NoNaNs);

private:
    size_t m_failedFrame = 0;
    size_t m_failedChannel = 0;
};

HART_MATCHER_DECLARE_ALIASES_FOR (NoNaNs)

}  // namespace hart
