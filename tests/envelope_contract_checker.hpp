#pragma once

#include <algorithm>
#include "hart.hpp"

class EnvelopeContractChecker :
    public hart::Envelope
{
public:
    EnvelopeContractChecker()
    {
        m_threadIdWhereInstanceWasConstructed = std::this_thread::get_id();
    }

    EnvelopeContractChecker& withExpectedMaxBlockSize (size_t maxBlockSizeFrames)
    {
        m_expectedMaxBlockSizeFrames = maxBlockSizeFrames;
        return *this;
    }

    EnvelopeContractChecker& withExpectedSampleRate (double sampleRateHz)
    {
        m_expectedSampleRateHz = sampleRateHz;
        return *this;
    }

    EnvelopeContractChecker& withExpectedRenderedDuration (double durationSeconds)
    {
        m_expectedRenderedDurationSeconds = durationSeconds;
        return *this;
    }

    void renderNextBlock (size_t blockSize, std::vector<double>& valuesOutput) override
    {
        HART_EXPECT_EQ (std::this_thread::get_id(), m_threadIdWhereInstanceWasConstructed);
        HART_EXPECT_TRUE (m_prepareCalled);

        HART_EXPECT_LE (blockSize, m_expectedMaxBlockSizeFrames);
        HART_ASSERT_GE (valuesOutput.size(), blockSize);

        std::fill (valuesOutput.begin(), valuesOutput.begin() + blockSize, 0.0);

        ++m_numRenderNextBlockCalls;
        m_numFramesRendered += blockSize;

        if (m_numFramesRendered >= m_expectedNumFramesRendered)
            verify();
    }

    void prepare (double sampleRateHz, size_t maxBlockSizeFrames) override
    {
        HART_EXPECT_EQ (std::this_thread::get_id(), m_threadIdWhereInstanceWasConstructed);

        HART_EXPECT_FLOAT_EQ (sampleRateHz, m_expectedSampleRateHz, 1e-8);
        HART_EXPECT_EQ (maxBlockSizeFrames, m_expectedMaxBlockSizeFrames);

        m_expectedNumFramesRendered = hart::roundToSizeT (m_expectedSampleRateHz * m_expectedRenderedDurationSeconds);
        m_expectedNumRenderNextBlockCalls = static_cast<int> (std::ceil (static_cast<double> (m_expectedNumFramesRendered) / m_expectedMaxBlockSizeFrames));
        m_prepareCalled = true;
    }

    void reset() override
    {
        HART_EXPECT_EQ (std::this_thread::get_id(), m_threadIdWhereInstanceWasConstructed);
    }

    std::unique_ptr<Envelope> copy() const override
    {
        return hart::make_unique<EnvelopeContractChecker> (*this);
    }

private:
    size_t m_expectedMaxBlockSizeFrames = hart::CLIConfig::getInstance().getGefaultBlockSizeFrames();
    double m_expectedSampleRateHz = hart::CLIConfig::getInstance().getDefaultSampleRateHz();
    double m_expectedRenderedDurationSeconds = hart::CLIConfig::getInstance().getDefaultRenderDurationSeconds();
    size_t m_expectedNumFramesRendered = 0;
    size_t m_expectedNumRenderNextBlockCalls = 0;

    bool m_prepareCalled = false;

    std::thread::id m_threadIdWhereInstanceWasConstructed {};
    size_t m_numRenderNextBlockCalls = 0;
    size_t m_numFramesRendered = 0;

    void verify() const
    {
        const double observedRenderedDurationSeconds = static_cast<double> (m_numFramesRendered) / m_expectedSampleRateHz;
        HART_EXPECT_FLOAT_EQ (observedRenderedDurationSeconds, m_expectedRenderedDurationSeconds, 5e-4);
        HART_EXPECT_EQ (m_numFramesRendered, m_expectedNumFramesRendered);
        HART_ASSERT_EQ (m_numRenderNextBlockCalls, m_expectedNumRenderNextBlockCalls);
    }
};
