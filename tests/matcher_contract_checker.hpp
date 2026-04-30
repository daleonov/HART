#pragma once

#include <thread>  // get_id()
#include "hart.hpp"

class MatcherContractChecker :
    public hart::Matcher<float, MatcherContractChecker>
{
public:
    MatcherContractChecker()
    {
        m_threadIdWhereInstanceWasConstructed = std::this_thread::get_id();
    }

    MatcherContractChecker& withExpectedMaxBlockSize (size_t maxBlockSizeFrames)
    {
        m_expectedMaxBlockSizeFrames = maxBlockSizeFrames;
        return *this;
    }

    MatcherContractChecker& withExpectedChannelLayout (size_t numInputChannels, size_t numOutputChannels)
    {
        m_expectedNumInputChannels = numInputChannels;
        m_expectedNumOutputChannels = numOutputChannels;
        return *this;
    }

    MatcherContractChecker& withExpectedSampleRate (double sampleRateHz)
    {
        m_expectedSampleRateHz = sampleRateHz;
        return *this;
    }

    MatcherContractChecker& withExpectedCheckedDuration (double durationSeconds)
    {
        m_expectedCheckedDurationSeconds = durationSeconds;
        return *this;
    }

    MatcherContractChecker& withMatchResult (bool shouldPass)
    {
        m_requestedMatchResult = shouldPass;
        return *this;
    }

    MatcherContractChecker& withPerBlockSupport (bool shouldBeAbleToOperatePerBlock)
    {
        m_shouldBeAbleToOperatePerBlock = shouldBeAbleToOperatePerBlock;
        return *this;
    }

    bool canOperatePerBlock() const override
    {
        HART_EXPECT_EQ (std::this_thread::get_id(), m_threadIdWhereInstanceWasConstructed);
        m_canOperatePerBlockCalled = true;
        return m_shouldBeAbleToOperatePerBlock;
    }

    void prepare (double sampleRateHz, size_t numInputChannels, size_t numOutputChannels, size_t maxBlockSizeFrames) override
    {
        // TODO: Enforce actual audio buffer size here for per-block matchers that are still called with full audio buffers

        HART_EXPECT_EQ (std::this_thread::get_id(), m_threadIdWhereInstanceWasConstructed);
        
        HART_EXPECT_TRUE (m_supportsSampleRateCalled);
        HART_EXPECT_FLOAT_EQ (sampleRateHz, m_expectedSampleRateHz, 1e-8);

        HART_EXPECT_TRUE (m_supportsChannelLayoutCalled);
        HART_EXPECT_EQ (numInputChannels, m_expectedNumInputChannels);
        HART_EXPECT_EQ (numOutputChannels, m_expectedNumOutputChannels);

        HART_EXPECT_EQ (maxBlockSizeFrames, m_expectedMaxBlockSizeFrames);

        m_expectedNumFramesChecked = hart::roundToSizeT (m_expectedSampleRateHz * m_expectedCheckedDurationSeconds);
        m_expectedNumMatchCalls = static_cast<int> (std::ceil (static_cast<double> (m_expectedNumFramesChecked) / m_expectedMaxBlockSizeFrames));
        m_prepareCalled = true;
    }

    bool match (const hart::AudioBuffer<float>& inputAudio, const hart::AudioBuffer<float>& observedOutputAudio) override
    {
        HART_EXPECT_EQ (std::this_thread::get_id(), m_threadIdWhereInstanceWasConstructed);
        HART_EXPECT_TRUE (m_prepareCalled);

        HART_EXPECT_LT (m_numFramesChecked, m_expectedNumFramesChecked);
        HART_EXPECT_LT (m_numMatchCalls, m_expectedNumMatchCalls);

        HART_EXPECT_EQ (inputAudio.getNumChannels(), m_expectedNumInputChannels);
        HART_EXPECT_EQ (observedOutputAudio.getNumChannels(), m_expectedNumOutputChannels);

        HART_EXPECT_EQ (inputAudio.getNumFrames(), observedOutputAudio.getNumFrames());

        HART_ASSERT_TRUE (inputAudio.hasSampleRate());
        HART_ASSERT_TRUE (observedOutputAudio.hasSampleRate());
        HART_ASSERT_FLOAT_EQ (inputAudio.getSampleRateHz(), observedOutputAudio.getSampleRateHz(), 1e-8);
        HART_ASSERT_FLOAT_EQ (inputAudio.getSampleRateHz(), m_expectedSampleRateHz, 1e-8);

        if (m_shouldBeAbleToOperatePerBlock)
            HART_EXPECT_LE (observedOutputAudio.getNumFrames(), m_expectedNumFramesChecked);  // Per-block matchers are not guaranteed per-block match() callbacks
        else
            HART_EXPECT_EQ (observedOutputAudio.getNumFrames(), m_expectedNumFramesChecked);

        ++m_numMatchCalls;
        m_numFramesChecked += observedOutputAudio.getNumFrames();

        if (m_numFramesChecked >= m_expectedNumFramesChecked)
            verify();

        return m_requestedMatchResult;
    }

    void reset() override
    {
        HART_EXPECT_EQ (std::this_thread::get_id(), m_threadIdWhereInstanceWasConstructed);
    }

    void represent (std::ostream& stream) const override
    {
        HART_EXPECT_EQ (std::this_thread::get_id(), m_threadIdWhereInstanceWasConstructed);
        stream << "MatcherContractChecker()";
    }

    bool supportsChannelLayout (size_t numInputChannels, size_t numOutputChannels) const override
    { 
        HART_EXPECT_EQ (std::this_thread::get_id(), m_threadIdWhereInstanceWasConstructed);
        HART_EXPECT_EQ (numInputChannels, m_expectedNumInputChannels);
        HART_EXPECT_EQ (numOutputChannels, m_expectedNumOutputChannels);

        m_supportsChannelLayoutCalled = true;
        return true;
    }

    bool supportsSampleRate (double sampleRateHz) const override
    {
        HART_EXPECT_EQ (std::this_thread::get_id(), m_threadIdWhereInstanceWasConstructed);
        HART_EXPECT_FLOAT_EQ (sampleRateHz, m_expectedSampleRateHz, 1e-8);

        m_supportsSampleRateCalled = true;
        return true;
    }

    hart::MatcherFailureDetails getFailureDetails() const override
    {
        HART_EXPECT_EQ (std::this_thread::get_id(), m_threadIdWhereInstanceWasConstructed);
        return {};
    }

private:
    size_t m_expectedMaxBlockSizeFrames = 1024;
    size_t m_expectedNumInputChannels = 1;
    size_t m_expectedNumOutputChannels = 1;
    double m_expectedSampleRateHz = 44100.0;
    double m_expectedCheckedDurationSeconds = 0.1;
    bool m_requestedMatchResult = true;
    bool m_shouldBeAbleToOperatePerBlock = false;
    int m_expectedNumMatchCalls = 0;
    size_t m_expectedNumFramesChecked = 0;

    mutable bool m_supportsSampleRateCalled = false;
    mutable bool m_supportsChannelLayoutCalled = false;
    mutable bool m_canOperatePerBlockCalled = false;
    bool m_prepareCalled = false;

    std::thread::id m_threadIdWhereInstanceWasConstructed {};
    int m_numMatchCalls = 0;
    size_t m_numFramesChecked = 0;
    
    void verify() const
    {
        const double observedCheckedDurationSeconds = static_cast<double> (m_numFramesChecked) / m_expectedSampleRateHz;
        HART_EXPECT_FLOAT_EQ (observedCheckedDurationSeconds, m_expectedCheckedDurationSeconds, 1e-4);
        HART_EXPECT_EQ (m_numFramesChecked, m_expectedNumFramesChecked);
        
        if (m_shouldBeAbleToOperatePerBlock)
            HART_EXPECT_TRUE (m_numMatchCalls == m_expectedNumMatchCalls || m_numMatchCalls == 1);
        else
            HART_EXPECT_EQ (m_numMatchCalls, 1);
    }
};
