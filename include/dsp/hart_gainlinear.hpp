#pragma once

#include <algorithm>  // fill()
#include <iomanip>
#include <vector>

#include "hart_dsp.hpp"
#include "hart_precision.hpp"

namespace hart
{

/// @brief Applies linear gain (not decibels) to the signal
/// @details To set gain in decibels, consider using @ref GainDb class instead
/// @ingroup DSP
template <typename SampleType>
class GainLinear:
    public hart::DSP<SampleType, GainLinear<SampleType>>
{
public:
    enum Params
    {
        gainLinear  ///< Linear gain
    };

    /// @brief Constructor
    /// @param initialGainLinear Linear gain
    GainLinear (double initialGainLinear = 1.0):
        m_initialGainLinear (initialGainLinear),
        m_gainLinear (initialGainLinear)
    {
    }

    void prepare (double /* sampleRateHz */, size_t /* numInputChannels */, size_t /* numOutputChannels */, size_t /* maxBlockSizeFrames */) override {}

    void process (const AudioBuffer<SampleType>& input, AudioBuffer<SampleType>& output, const EnvelopeBuffers& envelopeBuffers, ChannelFlags channelsToProcess) override
    {
        const size_t numInputChannels = input.getNumChannels();
        const size_t numOutputChannels = output.getNumChannels();
        hassert (output.getNumFrames() == input.getNumFrames());

        if (! supportsChannelLayout (numInputChannels, numOutputChannels))
            HART_THROW_OR_RETURN_VOID (hart::ChannelLayoutError, "Unsupported channel configuration");

        const bool hasGainEnvelope = ! envelopeBuffers.empty() && contains (envelopeBuffers, (int) Params::gainLinear);

        if (hasGainEnvelope)
            processEnvelopedGain (input, output, envelopeBuffers.at (GainLinear::gainLinear), channelsToProcess);
        else
            processConstantGain (input, output, channelsToProcess);
    }

    void reset() override {}

    /// @param id Only @ref GainLinear::gainLinear is accepted
    /// @param value linear gain
    void setValue (int id, double value) override
    {
        if (id == Params::gainLinear)
            m_gainLinear = value;
    }

    /// @param id Only @ref GainLinear::gainLinear is accepted
    /// @return linear gain
    double getValue (int id) const override
    {
        if (id == Params::gainLinear)
            return m_gainLinear;

        return 0.0;
    }

    /// @details Supports n-to-n configurations 
    virtual bool supportsChannelLayout (size_t numInputChannels, size_t numOutputChannels) const override
    {
        return numInputChannels == numOutputChannels;
    }

    virtual void represent (std::ostream& stream) const override
    {
        stream << dbPrecision << "GainLinear (" << m_initialGainLinear << ")";
    }

    /// @param id Only @ref GainLinear::gainLinear is accepted
    /// return true for GainLinear::gainLinear, else otherwise
    bool supportsEnvelopeFor (int id) const override
    {
        return id == Params::gainLinear;
    }

private:
    double m_initialGainLinear;
    double m_gainLinear;

    void processConstantGain (const AudioBuffer<SampleType>& input, AudioBuffer<SampleType>& output, ChannelFlags channelsToProcess)
    {
        for (size_t channel = 0; channel < input.getNumChannels(); ++channel)
        {
            if (channelsToProcess[channel] == true)
            {
                for (size_t frame = 0; frame < input.getNumFrames(); ++frame)
                    output[channel][frame] = input[channel][frame] * (SampleType) m_gainLinear;
            }
            else
            {
                for (size_t frame = 0; frame < input.getNumFrames(); ++frame)
                    output[channel][frame] = input[channel][frame];
            }
        }
    }

    void processEnvelopedGain (const AudioBuffer<SampleType>& input, AudioBuffer<SampleType>& output, const std::vector<double>& gainEnvelopeValues, ChannelFlags channelsToProcess)
    {
        for (size_t channel = 0; channel < input.getNumChannels(); ++channel)
        {
            if (channelsToProcess[channel] == true)
            {
                for (size_t frame = 0; frame < input.getNumFrames(); ++frame)
                    output[channel][frame] = input[channel][frame] * (SampleType) gainEnvelopeValues[frame];
            }
            else
            {
                for (size_t frame = 0; frame < input.getNumFrames(); ++frame)
                    output[channel][frame] = input[channel][frame];
            }

        }
    }
};

HART_DSP_DECLARE_ALIASES_FOR (GainLinear)

}  // namespace hart
