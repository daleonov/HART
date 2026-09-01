#pragma once

#include <cmath>  // pow()

#include "dsp/hart_dsp.hpp"
#include "hart_utils.hpp"  // Channel

/// @brief An effect that emulates unstable behaviour with growing (self-oscillating) decay
class UnstableDecay:
    public hart::DSP<float, UnstableDecay>
{
public:
    void process (const hart::AudioBuffer<float>& input, hart::AudioBuffer<float>& output, const hart::EnvelopeBuffers& /* envelopeBuffers */, hart::ChannelFlags /* channelsToProcess */) override
    {
        hassert (input.getNumChannels() == 1);
        hassert (output.getNumChannels() == 1);
        hassert (output.getNumFrames() == input.getNumFrames());
        
        constexpr float feedback = 1.0001f;

        const float* inputData = input[hart::Channel::left];
        float* outputData = output[hart::Channel::left];

        for (size_t i = 0; i < output.getNumFrames(); ++i)
        {
            m_state = inputData[i] + feedback * m_state;
            outputData[i] = m_state;
        }
    }

    void setValue (int /* id */, double /* value */) override {}
    void prepare (double /* sampleRateHz */, size_t /* numInputChannels */, size_t /* numOutputChannels */, size_t /* maxBlockSizeFrames */) override {}

    void reset() override
    {
        m_state = 0.0f;
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
        return hart::make_unique<UnstableDecay> (static_cast<const UnstableDecay&> (*this));
    }

    HART_DEFINE_GENERIC_REPRESENT (UnstableDecay);

private:
    float m_state = 0.0f;
};
