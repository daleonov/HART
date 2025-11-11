#pragma once

#include "hart_audio_buffer.hpp"
#include "hart_dsp.hpp"

#include <algorithm>  // min(), max()

namespace hart
{

template <typename SampleType>
class HardClip:
    public hart::DSP<SampleType, double>
{
public:
    enum Params
    {
        thresholdDb
    };

    HardClip (double thresholdDb = 0.0):
        m_initialThresholdDb (thresholdDb),
        m_thresholdLinear (decibelsToRatio (thresholdDb))
        {
        }

    void prepare (double /*sampleRateHz*/, size_t /*numInputChannels*/, size_t /*numOutputChannels*/, size_t /*maxBlockSizeFrames*/) override {}

    void process (const AudioBuffer<SampleType>& inputs, AudioBuffer<SampleType>& outputs) override
    {
        const size_t numChannels = inputs.getNumChannels();
        const size_t numFrames = inputs.getNumFrames();

        for (size_t channel = 0; channel < numChannels; ++channel)
            for (size_t frame = 0; frame < numFrames; ++frame)
                outputs[channel][frame] = std::min (std::max (inputs[channel][frame], (SampleType) -m_thresholdLinear), (SampleType) m_thresholdLinear);
    }

    void reset() override {}

    void setValue(int id, double value) override
    {
        if (id == Params::thresholdDb)
            m_thresholdLinear = decibelsToRatio (value);
    }

    bool supportsChannelLayout (size_t numInputChannels, size_t numOutputChannels) override
    {
        return numInputChannels == numOutputChannels;
    }

    void print (std::ostream& stream) const override
    {
        stream << "HardClip (" << m_initialThresholdDb << ")";
    }

private:
    double m_initialThresholdDb;
    double m_thresholdLinear;
};

}  // namespace hart
