#pragma once

#include <algorithm>  // fill()
#include <vector>

#include "hart_audio_buffer.hpp"
#include "hart_dsp.hpp"

namespace hart
{

/// @brief Applies linear gain (not decibels) to the signal
/// @details To set gain in decibels, consider using @ref GainDb class instead
/// @ingroup DSP
template <typename SampleType>
class GainLinear:
    public hart::DSP<SampleType>
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

    void process (const AudioBuffer<SampleType>& input, AudioBuffer<SampleType>& output, const EnvelopeBuffers& envelopeBuffers) override
    {
        const size_t numInputChannels = input.getNumChannels();
        const size_t numOutputChannels = output.getNumChannels();
        hassert (output.getNumFrames() == input.getNumFrames());

        if (! supportsChannelLayout (numInputChannels, numOutputChannels))
            HART_THROW_OR_RETURN_VOID (hart::ChannelLayoutError, "Unsupported channel configuration");

        const bool hasGainEnvelope = ! envelopeBuffers.empty() && contains (envelopeBuffers, (int) Params::gainLinear);
        const bool multiplexerMode = numInputChannels != numOutputChannels;

        if (hasGainEnvelope)
        {
            if (multiplexerMode)
                processEnvelopedGainAsMultiplexer (input, output, envelopeBuffers.at (GainLinear::gainLinear));
            else
                processEnvelopedGainAsMultiChannel (input, output, envelopeBuffers.at (GainLinear::gainLinear));

            return;
        }

        // No gain envelope
        if (multiplexerMode)
            processConstantGainAsMultiplexer (input, output);
        else
            processConstantGainAsMultiChannel (input, output);
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

    /// @details Supports either 1-to-n or n-to-n configurations 
    virtual bool supportsChannelLayout (size_t numInputChannels, size_t numOutputChannels) const override
    {
        if (numInputChannels == numOutputChannels)
            return true;

        if (numInputChannels == 1)
            return true;

        return false;
    }

    virtual void print (std::ostream& stream) const override
    {
        stream << "GainLinear (" << m_initialGainLinear << ")";
    }

    /// @param id Only @ref GainLinear::gainLinear is accepted
    /// return true for GainLinear::gainLinear, else otherwise
    bool supportsEnvelopeFor (int id) const override
    {
        return id == Params::gainLinear;
    }

    HART_DSP_DECLARE_COPY_METHOD (GainLinear);

private:
    double m_initialGainLinear;
    double m_gainLinear;

    void processConstantGainAsMultiChannel (const AudioBuffer<SampleType>& input, AudioBuffer<SampleType>& output)
    {
        for (size_t channel = 0; channel < input.getNumChannels(); ++channel)
            for (size_t frame = 0; frame < input.getNumFrames(); ++frame)
                output[channel][frame] = input[channel][frame] * m_gainLinear;
    }

    void processConstantGainAsMultiplexer (const AudioBuffer<SampleType>& input, AudioBuffer<SampleType>& output)
    {
        for (size_t channel = 0; channel < output.getNumChannels(); ++channel)
            for (size_t frame = 0; frame < input.getNumFrames(); ++frame)
                output[channel][frame] = input[0][frame] * m_gainLinear;
    }

    void processEnvelopedGainAsMultiChannel (const AudioBuffer<SampleType>& input, AudioBuffer<SampleType>& output, const std::vector<double>& gainEnvelopeValues)
    {
        for (size_t channel = 0; channel < input.getNumChannels(); ++channel)
            for (size_t frame = 0; frame < input.getNumFrames(); ++frame)
                output[channel][frame] = input[channel][frame] * gainEnvelopeValues[frame];
    }

    void processEnvelopedGainAsMultiplexer (const AudioBuffer<SampleType>& input, AudioBuffer<SampleType>& output, const std::vector<double>& gainEnvelopeValues)
    {
        for (size_t channel = 0; channel < output.getNumChannels(); ++channel)
            for (size_t frame = 0; frame < input.getNumFrames(); ++frame)
                output[channel][frame] = input[0][frame] * gainEnvelopeValues[frame];
    }
};

}  // namespace hart
