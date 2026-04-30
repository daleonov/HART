#pragma once

#include <cmath>  // round(), ceil()
#include <thread>  // get_id();

#include "hart.hpp"

/// @brief Verifies the correct order of callbacks, render behaviour and single-threadiness of callbacks
/// @private
class DSPContractChecker :
public hart::DSP<float, DSPContractChecker>
{
public:
    DSPContractChecker()
    {
        m_threadIdWhereInstanceWasConstructed = std::this_thread::get_id();
    }

    DSPContractChecker& withExpectedMaxBlockSize (size_t maxBlockSizeFrames)
    {
        m_expectedMaxBlockSizeFrames = maxBlockSizeFrames;
        return *this;
    }

    DSPContractChecker& withExpectedChannelLayout (size_t numInputChannels, size_t numOutputChannels)
    {
        m_expectedNumInputChannels = numInputChannels;
        m_expectedNumOutputChannels = numOutputChannels;
        return *this;
    }

    DSPContractChecker& withExpectedSampleRate (double sampleRateHz)
    {
        m_expectedSampleRateHz = sampleRateHz;
        return *this;
    }

    DSPContractChecker& withExpectedRenderedDuration (double durationSeconds)
    {
        m_expectedRenderedDurationSeconds = durationSeconds;
        return *this;
    }

    void verify() const
    {
        const double observedRenderedDurationSeconds = static_cast<double> (m_numFramesRendered) / m_expectedSampleRateHz;
        HART_EXPECT_FLOAT_EQ (observedRenderedDurationSeconds, m_expectedRenderedDurationSeconds, 1e-4);

        const size_t expectedNumFramesRendered = hart::roundToSizeT (m_expectedSampleRateHz * m_expectedRenderedDurationSeconds);
        HART_EXPECT_EQ (m_numFramesRendered, expectedNumFramesRendered);

        const int expectedNumProcessCalls = static_cast<int> (std::ceil (static_cast<double> (expectedNumFramesRendered) / m_expectedMaxBlockSizeFrames));
        HART_EXPECT_EQ (m_numProcessCalls, expectedNumProcessCalls);
    }

    void prepare (double sampleRateHz, size_t numInputChannels, size_t numOutputChannels, size_t maxBlockSizeFrames) override
    {
        HART_EXPECT_EQ (std::this_thread::get_id(), m_threadIdWhereInstanceWasConstructed);

        HART_EXPECT_TRUE (m_supportsSampleRateCalled);
        HART_EXPECT_FLOAT_EQ (sampleRateHz, m_expectedSampleRateHz, 1e-8);

        HART_EXPECT_TRUE (m_supportsChannelLayoutCalled);
        HART_EXPECT_EQ (numInputChannels, m_expectedNumInputChannels);
        HART_EXPECT_EQ (numOutputChannels, m_expectedNumOutputChannels);

        HART_EXPECT_EQ (maxBlockSizeFrames, m_expectedMaxBlockSizeFrames);

        m_prepareCalled = true;
    }

    void process (const hart::AudioBuffer<float>& input, hart::AudioBuffer<float>& output, const hart::EnvelopeBuffers& /* envelopeBuffers */, hart::ChannelFlags /* channelsToProcess */) override
    {
        HART_EXPECT_EQ (std::this_thread::get_id(), m_threadIdWhereInstanceWasConstructed);
        HART_EXPECT_TRUE (m_prepareCalled);

        HART_EXPECT_EQ (input.getNumChannels(), m_expectedNumInputChannels);
        HART_EXPECT_EQ (output.getNumChannels(), m_expectedNumOutputChannels);

        HART_EXPECT_EQ (input.getNumFrames(), output.getNumFrames());
        HART_EXPECT_LE (input.getNumFrames(), m_expectedMaxBlockSizeFrames);

        HART_ASSERT_TRUE (input.hasSampleRate());
        HART_ASSERT_TRUE (output.hasSampleRate());
        HART_ASSERT_FLOAT_EQ (input.getSampleRateHz(), output.getSampleRateHz(), 1e-8);
        HART_ASSERT_FLOAT_EQ (input.getSampleRateHz(), m_expectedSampleRateHz, 1e-8);

        output.clear();

        ++m_numProcessCalls;
        m_numFramesRendered += output.getNumFrames();
    }

    void reset() override
    {
        HART_EXPECT_EQ (std::this_thread::get_id(), m_threadIdWhereInstanceWasConstructed);
    }

    void setValue (int /* id */, double /* value */) override
    {
        HART_EXPECT_EQ (std::this_thread::get_id(), m_threadIdWhereInstanceWasConstructed);
    }

    bool supportsChannelLayout (size_t numInputChannels, size_t numOutputChannels) const override
    {
        HART_EXPECT_EQ (std::this_thread::get_id(), m_threadIdWhereInstanceWasConstructed);
        HART_EXPECT_EQ (numInputChannels, m_expectedNumInputChannels);
        HART_EXPECT_EQ (numOutputChannels, m_expectedNumOutputChannels);

        m_supportsChannelLayoutCalled = true;
        return true;
    }

    bool supportsEnvelopeFor (int /* id */) const override
    {
        HART_EXPECT_EQ (std::this_thread::get_id(), m_threadIdWhereInstanceWasConstructed);
        return true;
    }

    bool supportsSampleRate (double sampleRateHz) const override
    {
        HART_EXPECT_EQ (std::this_thread::get_id(), m_threadIdWhereInstanceWasConstructed);
        HART_EXPECT_FLOAT_EQ (sampleRateHz, m_expectedSampleRateHz, 1e-8);

        m_supportsSampleRateCalled = true;
        return true;
    }

    virtual void represent(std::ostream& stream) const override
    {
        HART_EXPECT_EQ (std::this_thread::get_id(), m_threadIdWhereInstanceWasConstructed);
        stream << "DSPContractChecker()"; 
    }

private:
    size_t m_expectedMaxBlockSizeFrames = 1024;
    size_t m_expectedNumInputChannels = 1;
    size_t m_expectedNumOutputChannels = 1;
    double m_expectedSampleRateHz = 44100.0;
    double m_expectedRenderedDurationSeconds = 0.1;

    mutable bool m_supportsSampleRateCalled = false;
    mutable bool m_supportsChannelLayoutCalled = false;
    bool m_prepareCalled = false;

    std::thread::id m_threadIdWhereInstanceWasConstructed {};
    int m_numProcessCalls = 0;
    size_t m_numFramesRendered = 0;
};
