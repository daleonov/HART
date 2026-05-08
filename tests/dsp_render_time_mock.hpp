#pragma once

#include <thread>
#include <chrono>

#include "hart.hpp"

/// @brief Emulates a DSP taking requested amount of time to render audio
/// @note The instantiation of this object is relatively expensive, since it runs
/// a CPU cycle vs real-world time calibration, so try to re-use it wherever possible.
/// @private
class DSPRenderTimeMock :
public hart::DSP<float, DSPRenderTimeMock>
{
public:
    DSPRenderTimeMock (double timeToRenderEachSampleSeconds = 1e-9) :
        m_timeToRenderEachSampleSeconds (timeToRenderEachSampleSeconds)
    {
    }

    void prepare (double /* sampleRateHz */, size_t /* numInputChannels */, size_t /* numOutputChannels */, size_t /* maxBlockSizeFrames */) override
    {
    }

    void process (const hart::AudioBuffer<float>& /* input */, hart::AudioBuffer<float>& output, const hart::EnvelopeBuffers& /* envelopeBuffers */, hart::ChannelFlags /* channelsToProcess */) override
    {
        const double requiredRenderTimeSeconds = output.getNumFrames() * output.getNumChannels() * m_timeToRenderEachSampleSeconds;
        std::this_thread::sleep_for (std::chrono::duration<double> (requiredRenderTimeSeconds));
        output.clear();
    }

    void setValue (int /* id */, double /* value */) override
    {
    }

    bool supportsChannelLayout (size_t /* numInputChannels */, size_t /* numOutputChannels */) const override
    {
        return true;
    }

    bool supportsEnvelopeFor (int /* id */) const override
    {
        return true;
    }

    bool supportsSampleRate (double /* sampleRateHz */) const override
    {
        return true;
    }

    void represent (std::ostream& stream) const override
    {
        stream
            << "DSPRenderTimeMock ("
            << hart::secPrecision << m_timeToRenderEachSampleSeconds
            << "_s)"; 
    }

private:
    const double m_timeToRenderEachSampleSeconds;
};
