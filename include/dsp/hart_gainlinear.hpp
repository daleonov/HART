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
        m_gainEnvelopeValues.resize (maxBlockSizeFrames);
    }

    void process (const AudioBuffer<SampleType>& inputs, AudioBuffer<SampleType>& outputs) override
    {
        const size_t numInputChannels = inputs.getNumChannels();
        const size_t numOutputChannels = outputs.getNumChannels();
        // TODO: Check if number of frames is equal

        const size_t blockSize = inputs.getNumFrames();
        getValues (Params::gainLinear, blockSize, m_gainEnvelopeValues);

        if (numInputChannels == numOutputChannels)
        {
            for (size_t channel = 0; channel < inputs.getNumChannels(); ++channel)
                for (size_t frame = 0; frame < blockSize; ++frame)
                    outputs[channel][frame] = inputs[channel][frame] * m_gainEnvelopeValues[frame];

            return;
        }

        if (numInputChannels == 1)
        {
            for (size_t channel = 0; channel < outputs.getNumChannels(); ++channel)
                for (size_t frame = 0; frame < blockSize; ++frame)
                    outputs[channel][frame] = inputs[0][frame] * m_gainEnvelopeValues[frame];

            return;
        }

        // If we're here, the channel layout is unsupported
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
};

}  // namespace hart
