#pragma once

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
        m_timeToRenderEachSampleSeconds (timeToRenderEachSampleSeconds),
        m_iterationsPerSecond (measureIterationsPerSecond())
    {
    }

    void prepare (double /* sampleRateHz */, size_t /* numInputChannels */, size_t /* numOutputChannels */, size_t /* maxBlockSizeFrames */) override
    {
    }

    void process (const hart::AudioBuffer<float>& /* input */, hart::AudioBuffer<float>& output, const hart::EnvelopeBuffers& /* envelopeBuffers */, hart::ChannelFlags /* channelsToProcess */) override
    {
        const double requiredRenderTimeSeconds = output.getNumFrames() * output.getNumChannels() * m_timeToRenderEachSampleSeconds;
        const size_t numIterations = hart::roundToSizeT (requiredRenderTimeSeconds * m_iterationsPerSecond);

        volatile size_t x = 0;  // To make sure the compiler doesn't optimize away the loop below

        for (size_t i = 0; i < numIterations; ++i)
            x += i;

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
    const double m_iterationsPerSecond;

    static double measureIterationsPerSecond()
    {
        using clock = std::chrono::steady_clock;
        using std::chrono::duration_cast;
        using duration_double = std::chrono::duration<double>;

        constexpr size_t numIterations = 1000000;

        const clock::time_point start = std::chrono::steady_clock::now();
        volatile size_t x = 0;  // To make sure the compiler doesn't optimize away the loop below

        for (size_t i = 0; i < numIterations; ++i)
            x += i;

        const clock::time_point end = std::chrono::steady_clock::now();
        const double durationSeconds = duration_cast<duration_double> (end - start).count();
        return static_cast<double> (numIterations) / durationSeconds;
    }
};
