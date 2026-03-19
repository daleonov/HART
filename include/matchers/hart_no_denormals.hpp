#pragma once

#include <cmath>  // fabs(), isnormal(), isinf(), isnan()
#include <iomanip>
#include <sstream>

#include "matchers/hart_matcher.hpp"
#include "hart_precision.hpp"

namespace hart
{

/// @brief Checks whether the audio has no denormal values
/// @details This matcher scans the audio signal for denormal (subnormal) floating-point values.
/// @ingroup Matchers
template<typename SampleType>
class NoDenormals:
    public Matcher<SampleType, NoDenormals<SampleType>>
{
public:
    bool match (const AudioBuffer<SampleType>& /* inputAudio */, const AudioBuffer<SampleType>& observedOutputAudio) override
    {
        for (size_t channel = 0; channel < observedOutputAudio.getNumChannels(); ++channel)
        {
            if (! this->appliesToChannel (channel))
                continue;

            for (size_t frame = 0; frame < observedOutputAudio.getNumFrames(); ++frame)
            {
                const SampleType sample = observedOutputAudio[channel][frame];
                const bool notZero = std::fabs (sample) > SampleType (0);

                if (notZero && ! std::isnormal (sample) && ! std::isinf (sample) && ! std::isnan (sample))
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
        stream
            << "Denormal value "
            << std::scientific << std::setprecision (3)
            << m_observedValue
            << " found";

        MatcherFailureDetails details;
        details.frame = m_failedFrame;
        details.channel = m_failedChannel;
        details.description = stream.str();

        return details;
    }

    bool canOperatePerBlock() override
    {
        return true;
    }

    void reset() override {}
    void prepare (double /*sampleRateHz*/, size_t /* numChannels */, size_t /* maxBlockSizeFrames */) override {}

    HART_DEFINE_GENERIC_REPRESENT (NoDenormals);

private:
    size_t m_failedFrame = 0;
    size_t m_failedChannel = 0;
    SampleType m_observedValue = SampleType (0);
};

HART_MATCHER_DECLARE_ALIASES_FOR (NoDenormals)

}  // namespace hart
