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
    public hart::DSP<SampleType>
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
        m_gainEnvelopeValues.resize (this->hasEnvelopeFor (Params::gainDb) ? maxBlockSizeFrames : 0);
    }

    void process (const AudioBuffer<SampleType>& input, AudioBuffer<SampleType>& output) override
    {
        const size_t numInputChannels = input.getNumChannels();
        const size_t numOutputChannels = output.getNumChannels();
        hassert (output.getNumFrames() == input.getNumFrames());

        if (! supportsChannelLayout (numInputChannels, numOutputChannels))
            HART_THROW_OR_RETURN_VOID (hart::ChannelLayoutError, "Unsupported channel configuration");

        const bool hasEnvelope = ! m_gainEnvelopeValues.empty();;
        const bool multiplexerMode = numInputChannels != numOutputChannels;

        if (hasEnvelope)
        {
            this->getValues (Params::gainDb, input.getNumFrames(), m_gainEnvelopeValues);
            
            for (size_t i = 0; i < m_gainEnvelopeValues.size(); ++i)
                m_gainEnvelopeValues[i] = decibelsToRatio (m_gainEnvelopeValues[i]);

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

    HART_DSP_DECLARE_COPY_METHOD (GainDb);

private:
    double m_initialGainDb;
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
