#pragma once

#include <algorithm>  // fill()
#include <vector>

#include "hart_audio_buffer.hpp"
#include "hart_dsp.hpp"

namespace hart
{

template <typename SampleType>
class GainLinear:
    public hart::DSP<typename SampleType>
{
public:
    enum Params
    {
        gainLinear
    };

    GainLinear (double initialGainLinear = 1.0):
        m_initialGainLinear (initialGainLinear),
        m_gainLinear (initialGainLinear)
    {
    }

    void prepare (double /* sampleRateHz */, size_t /* numInputChannels */, size_t /* numOutputChannels */, size_t maxBlockSizeFrames) override
    {
        m_gainEnvelopeValues.resize (hasEnvelopeFor (Params::gainLinear) ? maxBlockSizeFrames : 0);
    }

    void process (const AudioBuffer<SampleType>& input, AudioBuffer<SampleType>& output) override
    {
        const size_t numInputChannels = input.getNumChannels();
        const size_t numOutputChannels = output.getNumChannels();
        // TODO: Check if number of frames is equal

        if (! supportsChannelLayout (numInputChannels, numOutputChannels))
        {
            // TODO: assert
            return;
        }

        const bool hasEnvelope = ! m_gainEnvelopeValues.empty();
        const bool multiplexerMode = numInputChannels != numOutputChannels;

        if (hasEnvelope)
        {
            getValues (Params::gainLinear, input.getNumFrames(), m_gainEnvelopeValues);

            if (multiplexerMode)
                processEnvelopedGainAsMultiplexer (input, output);
            else
                processEnvelopedGainAsMultiChannel (input, output);

            return;
        }

        // No gain envelope
        if (multiplexerMode)
            processConstantGainAsMultiplexer (input, output);
        else
            processConstantGainAsMultiChannel (input, output);
    }

    void reset() override {}

    void setValue (int id, double value) override
    {
        if (id == Params::gainLinear)
            m_gainLinear = value;
    }

    double getValue (int id) const override
    {
        if (id == Params::gainLinear)
            return m_gainLinear;

        return 0.0;
    }

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

    bool supportsEnvelopeFor (int id) const override
    {
        return id == Params::gainLinear;
    }

    HART_DSP_DECLARE_COPY_METHOD (GainLinear);

private:
    double m_initialGainLinear;
    double m_gainLinear;
    std::vector<double> m_gainEnvelopeValues;

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

    void processEnvelopedGainAsMultiChannel (const AudioBuffer<SampleType>& input, AudioBuffer<SampleType>& output)
    {
        for (size_t channel = 0; channel < input.getNumChannels(); ++channel)
            for (size_t frame = 0; frame < input.getNumFrames(); ++frame)
                output[channel][frame] = input[channel][frame] * m_gainEnvelopeValues[frame];
    }

    void processEnvelopedGainAsMultiplexer (const AudioBuffer<SampleType>& input, AudioBuffer<SampleType>& output)
    {
        for (size_t channel = 0; channel < output.getNumChannels(); ++channel)
            for (size_t frame = 0; frame < input.getNumFrames(); ++frame)
                output[channel][frame] = input[0][frame] * m_gainEnvelopeValues[frame];
    }
};

}  // namespace hart
