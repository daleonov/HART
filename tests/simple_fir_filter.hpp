#pragma once

#include "dsp/hart_dsp.hpp"
#include "hart_utils.hpp"  // Channel

/// @brief A very simple FIR filter, for testing purposes
class SimpleFIRFilter:
    public hart::DSP<float, SimpleFIRFilter>
{
public:
    void process (const hart::AudioBuffer<float>& input, hart::AudioBuffer<float>& output, const hart::EnvelopeBuffers& /* envelopeBuffers */, hart::ChannelFlags /* channelsToProcess */) override
    {
        hassert (input.getNumChannels() == 1);
        hassert (output.getNumChannels() == 1);
        hassert (output.getNumFrames() == input.getNumFrames());

        const float* inputData = input[hart::Channel::left];
        float* outputData = output[hart::Channel::left];

        for (size_t i = 0; i < output.getNumFrames(); ++i)
        {
            const float current = inputData[i];
            outputData[i] = current * 0.123f + m_previous * 0.456f;
            m_previous = current;
        }
    }

    void setValue (int /* id */, double /* value */) override {}
    void prepare (double /* sampleRateHz */, size_t /* numInputChannels */, size_t /* numOutputChannels */, size_t /* maxBlockSizeFrames */) override {}

    void reset() override
    {
        m_previous = 0.0f;
    }

    virtual bool supportsChannelLayout (size_t numInputChannels, size_t numOutputChannels) const override
    {
        return numInputChannels == 1 && numOutputChannels == 1;
    }

    bool supportsEnvelopeFor (int /* id */) const override
    {
        return false;
    }

    std::unique_ptr<hart::DSPBase<float>> copy() const override
    {
        return hart::make_unique<SimpleFIRFilter> (static_cast<const SimpleFIRFilter&> (*this));
    }

    HART_DEFINE_GENERIC_REPRESENT (SimpleFIRFilter);

private:
    float m_previous = 0.0f;
};
