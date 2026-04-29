#pragma once

#include <thread>  // get_id()
#include "hart.hpp"

class SignalContractChecker :
    public hart::Signal<float, SignalContractChecker>
{
public:
    SignalContractChecker()
    {
        m_threadIdWhereInstanceWasConstructed = std::this_thread::get_id();
    }

    SignalContractChecker& withExpectedMaxBlockSize (size_t maxBlockSizeFrames)
    {
        m_expectedMaxBlockSizeFrames = maxBlockSizeFrames;
        return *this;
    }

    SignalContractChecker& withExpectedNumChannels (size_t numChannels)
    {
        m_expectedNumChannels = numChannels;
        return *this;
    }

    SignalContractChecker& withExpectedSampleRate (double sampleRateHz)
    {
        m_expectedSampleRateHz = sampleRateHz;
        return *this;
    }

    SignalContractChecker& withExpectedRenderedDuration (double durationSeconds)
    {
        m_expectedRenderedDurationSeconds = durationSeconds;
        return *this;
    }

    void verify() const
    {

    }

    bool supportsNumChannels (size_t numChannels) const override
    {
        HART_EXPECT_EQ (std::this_thread::get_id(), m_threadIdWhereInstanceWasConstructed);
        HART_EXPECT_EQ (numChannels, m_expectedNumChannels);

        m_supportsNumChannelsCalled = true;
        return true;
    }

    bool supportsSampleRate (double sampleRateHz) const override
    {
        HART_EXPECT_EQ (std::this_thread::get_id(), m_threadIdWhereInstanceWasConstructed);
        HART_EXPECT_FLOAT_EQ (sampleRateHz, m_expectedSampleRateHz, 1e-8);

        m_supportsSampleRateCalled = true;
        return true;
    }

    void prepare (double sampleRateHz, size_t numOutputChannels, size_t maxBlockSizeFrames) override
    {
        HART_EXPECT_EQ (std::this_thread::get_id(), m_threadIdWhereInstanceWasConstructed);

        HART_EXPECT_TRUE (m_supportsNumChannelsCalled);
        HART_EXPECT_EQ (numOutputChannels, m_expectedNumChannels);

        HART_EXPECT_TRUE (m_supportsSampleRateCalled);
        HART_EXPECT_FLOAT_EQ (sampleRateHz, m_expectedSampleRateHz, 1e-8);

        HART_EXPECT_EQ (maxBlockSizeFrames, m_expectedMaxBlockSizeFrames);

        m_prepareCalled = true;
    }

    void renderNextBlock (hart::AudioBuffer<float>& output) override
    {
        HART_EXPECT_EQ (std::this_thread::get_id(), m_threadIdWhereInstanceWasConstructed);
        HART_EXPECT_TRUE (m_prepareCalled);

        HART_EXPECT_EQ (output.getNumChannels(), m_expectedNumChannels);
        HART_EXPECT_LE (output.getNumFrames(), m_expectedMaxBlockSizeFrames);
        HART_EXPECT_TRUE (output.hasSampleRate());
        HART_EXPECT_FLOAT_EQ (output.getSampleRateHz(), m_expectedSampleRateHz, 1e-8);

        ++m_numrenderNextBlockCalls;
        m_numFramesRendered += output.getNumFrames();
    }

    void represent (std::ostream& stream) const override
    {
        HART_EXPECT_EQ (std::this_thread::get_id(), m_threadIdWhereInstanceWasConstructed);
        stream << "SignalContractChecker()"; 
    }

    void reset() override
    {
    }

private:
    size_t m_expectedMaxBlockSizeFrames = 1024;
    size_t m_expectedNumChannels = 1;
    double m_expectedSampleRateHz = 44100.0;
    double m_expectedRenderedDurationSeconds = 0.1;

    mutable bool m_supportsSampleRateCalled = false;
    mutable bool m_supportsNumChannelsCalled = false;
    bool m_prepareCalled = false;

    std::thread::id m_threadIdWhereInstanceWasConstructed {};
    int m_numrenderNextBlockCalls = 0;
    size_t m_numFramesRendered = 0;
};
