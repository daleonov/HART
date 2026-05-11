#pragma once

#include <cmath>
#include <memory>

#include "hart_exceptions.hpp"
#include "hart_precision.hpp"
#include "signals/hart_signal.hpp"
#include "hart_utils.hpp"

namespace hart
{

/// @brief Produces a Nyquist signal
/// @details Regardless of sample rate, outputs an alternating squence like:
/// {+1, -1, +1, -1, +1, ...} at 0 dB sample peak.
/// @ingroup Signals
template<typename SampleType>
class NyquistSignal:
    public Signal<SampleType, NyquistSignal<SampleType>>
{
public:
    NyquistSignal() = default;
    bool supportsNumChannels (size_t /* numChannels */) const override { return true; };
    void prepare (double /* sampleRateHz */, size_t /* numOutputChannels */, size_t /*maxBlockSizeFrames*/) override {}

    void renderNextBlock (AudioBuffer<SampleType>& output) override
    {
        hassert (output.getNumChannels() >= 1);
        const size_t numFrames = output.getNumFrames();

        for (size_t channel = 0; channel < output.getNumChannels(); ++channel)
        {
            SampleType* channelData = output[channel];

            const SampleType evenFrameValues = m_nextBlockStartValue;
            const SampleType oddFrameValues = -m_nextBlockStartValue;

            for (size_t frame = 0; frame < numFrames; frame += 2)
                channelData[frame] = evenFrameValues;

            for (size_t frame = 1; frame < numFrames; frame += 2)
                channelData[frame] = oddFrameValues;
        }

        if (numFrames % 2 != 0)
            m_nextBlockStartValue = -m_nextBlockStartValue;
    }

    void reset() override
    {
        m_nextBlockStartValue = 1.0;
    }

    HART_DEFINE_GENERIC_REPRESENT (NyquistSignal);

private:
    SampleType m_nextBlockStartValue = 1.0;
};

HART_SIGNAL_DECLARE_ALIASES_FOR (NyquistSignal)

}  // namespace hart
