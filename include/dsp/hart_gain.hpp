#pragma once

#include "hart_audio_buffer.hpp"
#include "hart_dsp.hpp"
#include "hart_utils.hpp"

namespace hart
{

template <typename SampleType>
class Gain:
    public hart::DSP<typename SampleType, double>
{
public:
    enum Params
    {
        gainDb
    };

    Gain (double gainDb = 0.0):
        m_initialGainDb (gainDb),
        m_gainLinear (decibelsToRatio (gainDb))
    {
    }

    void prepare (double /* sampleRateHz */, size_t /* numInputChannels */, size_t /* numOutputChannels */, size_t /* maxBlockSizeFrames */) override {};

    void process (const AudioBuffer<SampleType>& inputs, AudioBuffer<SampleType>& outputs) override
    {
        const size_t numInputChannels = inputs.getNumChannels();
        const size_t numOutputChannels = outputs.getNumChannels();
        // TODO: Check if number of frames is equal

        if (numInputChannels == numOutputChannels)
        {
            for (size_t channel = 0; channel < inputs.getNumChannels(); ++channel)
                for (size_t frame = 0; frame < inputs.getNumFrames(); ++frame)
                    outputs[channel][frame] = inputs[channel][frame] * m_gainLinear;

            return;
        }

        if (numInputChannels == 1)
        {
            for (size_t channel = 0; channel < outputs.getNumChannels(); ++channel)
                for (size_t frame = 0; frame < inputs.getNumFrames(); ++frame)
                    outputs[channel][frame] = inputs[0][frame] * m_gainLinear;

            return;
        }

        // If we're here, the channel layout is unsupported
    }

    void reset() override {}

    void setValue (int id, double value) override
    {
        if (id == Params::gainDb)
            m_gainLinear = decibelsToRatio (value);
    }

    virtual bool supportsChannelLayout (size_t numInputChannels, size_t numOutputChannels)
    {
        if (numInputChannels == numOutputChannels)
            return true;

        if (numInputChannels == 1)
            return true;

        return false;
    }

    virtual void print (std::ostream& stream) const override
    {
        stream << "Gain (" << m_initialGainDb << ")";
    }

private:
    double m_initialGainDb;
    double m_gainLinear;
};

}  // namespace hart
