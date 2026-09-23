#pragma once

#include <cmath>  // isinf()
#include <sstream>

#include "matchers/hart_matcher.hpp"

namespace hart
{

/// @brief Checks whether the audio has no infinity values
/// @details This matcher fails it at least one of the frames in the output audio contains `inf` value
/// @ingroup Matchers
template<typename SampleType>
class NoInfinities:
    public Matcher<SampleType, NoInfinities<SampleType>>
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

                if (std::isinf (sample))
                {
                    m_failedFrame = frame;
                    m_failedChannel = channel;
                    m_observedValue = sample;
                    return false;
                }
            }
        }

        return true;
    }

    MatcherFailureDetails getFailureDetails() const override
    {
        std::stringstream stream;
        stream << "Infinity value found: " << m_observedValue;

        MatcherFailureDetails details;
        details.frame = m_failedFrame;
        details.channel = m_failedChannel;
        details.description = stream.str();

        return details;
    }

    bool canOperatePerBlock() const override
    {
        return true;
    }

    void prepare (double /*sampleRateHz*/, size_t /* numInputChannels */, size_t /* numOutputChannels */, size_t /* maxBlockSizeFrames */) override {}

    HART_DEFINE_GENERIC_REPRESENT (NoInfinities);

private:
    size_t m_failedFrame = 0;
    size_t m_failedChannel = 0;
    SampleType m_observedValue = SampleType (0);
};

HART_MATCHER_DECLARE_ALIASES_FOR (NoInfinities)

}  // namespace hart
