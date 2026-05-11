#pragma once

#include <cmath>
#include <memory>

#include "hart_exceptions.hpp"
#include "hart_precision.hpp"
#include "signals/hart_signal.hpp"
#include "hart_utils.hpp"

namespace hart
{

/// @brief Produces a DC signal
/// @details Regardless of sample rate, assigns each sample to a requested value
/// @ingroup Signals
template<typename SampleType>
class DC:
    public Signal<SampleType, DC<SampleType>>
{
public:
    /// @brief Creates a DC Signal
    /// @param dcValueLinear Value to fill the buffers with, in linear domain, signed
    DC (SampleType dcValueLinear) :
        m_dcValueLinear (dcValueLinear)
    {
    }

    bool supportsNumChannels (size_t /* numChannels */) const override { return true; };
    void prepare (double /* sampleRateHz */, size_t /* numOutputChannels */, size_t /*maxBlockSizeFrames*/) override {}
    void reset() override {}

    void renderNextBlock (AudioBuffer<SampleType>& output) override
    {
        const size_t numChannels = output.getNumChannels();
        const size_t numFrames = output.getNumFrames();

        for (size_t channel = 0; channel < numChannels; ++channel)
        {
            SampleType* channelData = output[channel];

            for (size_t frame = 0; frame < output.getNumFrames(); ++frame)
                channelData[frame] = m_dcValueLinear;
        }
    }

    void represent (std::ostream& stream) const override
    {
        stream << "DC (" << linPrecision << m_dcValueLinear << ')';
    }

private:
    const SampleType m_dcValueLinear;
};

HART_SIGNAL_DECLARE_ALIASES_FOR (DC)

}  // namespace hart
