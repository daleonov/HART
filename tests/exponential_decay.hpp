#pragma once

#include <cmath>  // pow()

#include "dsp/hart_dsp.hpp"
#include "hart_utils.hpp"  // Channel

/// @brief A simple effect to test reverb decay related metrics, such as RT60.
/// @note Doesn't intent to do anything musical, it's more of a mathematical
/// "effect", meant to have an exponential impulse response, mimicking
/// what actual reverb would do, without implementing any reverb algorithms.
class ExponentialDecay:
    public hart::DSP<float, ExponentialDecay>
{
public:
    enum Params
    {
        decayTimeSeconds
    };

    void process (const hart::AudioBuffer<float>& input, hart::AudioBuffer<float>& output, const hart::EnvelopeBuffers& /* envelopeBuffers */, hart::ChannelFlags /* channelsToProcess */) override
    {
        hassert (input.getNumChannels() == 1);
        hassert (output.getNumChannels() == 1);
        hassert (output.getNumFrames() == input.getNumFrames());

        const float* inputData = input[hart::Channel::left];
        float* outputData = output[hart::Channel::left];

        for (size_t i = 0; i < output.getNumFrames(); ++i)
        {
            m_state = inputData[i] + m_feedback * m_state;
            outputData[i] = m_state;
        }
    }

    void setValue (int id, double value) override
    {
        if (id != Params::decayTimeSeconds)
        {
            hassertfalse;  // Unexpected param
            return;
        }

        m_decayTimeSeconds = value;
        calculateFeedback();
    }

    void prepare (double sampleRateHz, size_t /* numInputChannels */, size_t /* numOutputChannels */, size_t /* maxBlockSizeFrames */) override
    {
        hassert (sampleRateHz > 0.0);
        m_sampleRateHz = sampleRateHz;
        calculateFeedback();
    }

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
        return hart::make_unique<ExponentialDecay> (static_cast<const ExponentialDecay&> (*this));
    }

    HART_DEFINE_GENERIC_REPRESENT (ExponentialDecay);

private:
    double m_sampleRateHz = 44100.0;
    double m_decayTimeSeconds = 0.1;
    float m_state = 0.0f;
    float m_feedback = 0.0f;

    void calculateFeedback()
    {
        // After m_decayTimeSeconds, amplitude should be 0.001 (-60 dB)
        m_feedback = static_cast<float> (std::pow (10.0, -3.0 / static_cast<float> (m_decayTimeSeconds * m_sampleRateHz)));
    }
};
