#pragma once

#include "signals/hart_signal.hpp"
#include "hart_utils.hpp"

namespace hart
{

/// @brief Produces a {1, 0, 0, 0, ...} sequence
/// @ingroup Signals
template<typename SampleType>
class Impulse:
    public Signal<SampleType, Impulse<SampleType>>
{
public:
    /// @brief Creates an impulse signal instance
    Impulse() = default;

    void prepare (double /* sampleRateHz */, size_t /* numOutputChannels */, size_t /*maxBlockSizeFrames*/) override {}

    void renderNextBlock (AudioBuffer<SampleType>& output) override
    {        
        if (output.getNumChannels() == 0 || output.getNumFrames() == 0)
            return;

        output.clear();

        if (m_impulseWasAlreadyRendered)
            return;

        for (size_t channel = 0; channel < output.getNumChannels(); ++channel)
            output[channel][0] = (SampleType) 1;

        m_impulseWasAlreadyRendered = true;
    }

    void reset() override
    {
        m_impulseWasAlreadyRendered = false;
    }

    HART_DEFINE_GENERIC_REPRESENT (Impulse)

private:
    bool m_impulseWasAlreadyRendered = false;
};

HART_SIGNAL_DECLARE_ALIASES_FOR (Impulse)

}  // namespace hart
