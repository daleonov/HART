#pragma once

#include <algorithm>  // fill()
#include <vector>

#include "hart_audio_buffer.hpp"
#include "hart_dsp.hpp"
#include "hart_utils.hpp"

namespace hart
{

template <typename SampleType>
class GainDb:
    public hart::DSP<typename SampleType>
{
public:
    enum Params
    {
        gainDb
    };

    GainDb (double gainDb = 0.0):
        m_initialGainDb (gainDb),
        m_gainLinear (decibelsToRatio (gainDb))
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

        if (hasEnvelopeFor (Params::gainDb))
        {
            getValues (Params::gainDb, blockSize, m_gainEnvelopeValues);
            
            for (size_t i = 0; i < m_gainEnvelopeValues.size(); ++i)
                m_gainEnvelopeValues[i] = decibelsToRatio (m_gainEnvelopeValues[i]);
        }
        else
        {
            std::fill (m_gainEnvelopeValues.begin(), m_gainEnvelopeValues.end(), m_gainLinear);
        }

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
        if (id == Params::gainDb)
            m_gainLinear = decibelsToRatio (value);
    }

    double getValue (int id) const override
    {
        if (id == Params::gainDb)
            return ratioToDecibels (m_gainLinear);

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
        stream << "GainDb (" << m_initialGainDb << ")";
    }

    bool supportsEnvelopeFor (int id) const override
    {
        return id == Params::gainDb;
    }

private:
    double m_initialGainDb;
    double m_gainLinear;
    std::vector<double> m_gainEnvelopeValues;
};

}  // namespace hart
