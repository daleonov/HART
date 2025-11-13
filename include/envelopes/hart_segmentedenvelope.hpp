#pragma once

#include <cmath>  // pow(), abs()
#include <vector>

#include "envelopes/hart_envelope.hpp"
#include "hart_exceptions.hpp"
#include "hart_utils.hpp"

namespace hart
{

class SegmentedEnvelope:
    public Envelope
{
public:
    // TODO: Log curve?
    enum class Shape
    {
        linear,
        exponential,
        sCurve
    };

    SegmentedEnvelope (double startValue):
        m_resetValue (startValue),
        m_beginValue (startValue),
        m_endValue (startValue),
        m_currentValue (startValue)
    {
    }

    void renderNextBlock (size_t blockSize, std::vector<double>& valuesOutput) override
    {
        if (valuesOutput.size() != blockSize)
        {
            HART_WARNING ("Make sure to configure your envelope container size before processing audio");
            valuesOutput.resize (blockSize);
        }

        for (size_t i = 0; i < blockSize; ++i)
        {
            advance (m_frameTimeSeconds);
            valuesOutput[i] = m_currentValue;
        }
    }

    void prepare (double sampleRateHz, size_t /* maxBlockSizeFrames */) override
    {   
        if (sampleRateHz < 0 || floatsEqual (sampleRateHz, 0.0))
            HART_THROW_OR_RETURN_VOID (hart::ValueError, "Illegal sample rate value");

        m_frameTimeSeconds = 1.0 / sampleRateHz;
    }

    void reset() override
    {
        m_currentTimeSeconds = 0.0;
        m_currentSegmentIndex = 0;
        m_currentValue = m_resetValue;
    }

    SegmentedEnvelope& hold (double duration_s)
    {
        m_segments.push_back ({duration_s, m_endValue, Shape::linear, true});
        return *this;
    }

    SegmentedEnvelope& rampTo (double targetValue, double duration_s, Shape shape = Shape::linear)
    {
        m_segments.push_back ({duration_s, targetValue, shape, false});
        m_endValue = targetValue;
        return *this;
    }

    std::unique_ptr<Envelope> copy() const
    {
        return std::make_unique<SegmentedEnvelope> (*this);
    }

private:
    struct Segment
    {
        double durationSeconds;
        double targetValue;
        Shape shape = Shape::linear;
        bool isHold = false;
    };

    const double m_resetValue;
    double m_beginValue;
    double m_endValue;
    std::vector<Segment> m_segments;

    double m_currentTimeSeconds = 0.0;
    size_t m_currentSegmentIndex = 0;
    double m_currentValue;

    double m_frameTimeSeconds = 1.0 / 44100.0;

    void advance (double timeSeconds)
    {
        m_currentTimeSeconds += timeSeconds;

        // TODO: Less nesting here
        while (m_currentSegmentIndex < m_segments.size())
        {
            const Segment& currentSegment = m_segments[m_currentSegmentIndex];

            if (m_currentTimeSeconds < currentSegment.durationSeconds)
            {
                const double t = m_currentTimeSeconds / currentSegment.durationSeconds;

                if (currentSegment.isHold) 
                {
                    m_currentValue = currentSegment.targetValue;
                }
                else
                {
                    switch (currentSegment.shape)
                    {
                        case Shape::linear:
                        {
                            m_currentValue = m_beginValue + (currentSegment.targetValue - m_beginValue) * t;
                            break;
                        }

                        case Shape::exponential:
                        {
                            const double ratio = currentSegment.targetValue / m_beginValue;

                            if (std::abs (ratio - 1.0) < 1e-9)
                            {
                                m_currentValue = m_beginValue;
                                break;
                            }

                            const bool isFallingCurve = ratio > 1.0;

                            if (isFallingCurve)
                            {
                                double k = std::log (ratio) / currentSegment.durationSeconds;
                                m_currentValue = m_beginValue * std::exp (k * m_currentTimeSeconds);
                            }
                            else
                            {
                                double k = std::log (1.0 / ratio) / currentSegment.durationSeconds;
                                m_currentValue = m_beginValue * std::exp (-k * m_currentTimeSeconds);
                            }

                            break;
                        }

                        case Shape::sCurve:
                        {
                            const double smoothstep = t * t * (3.0 - 2.0 * t);
                            m_currentValue = m_beginValue + (currentSegment.targetValue - m_beginValue) * smoothstep;
                            break;
                        }
                    }
                }

                break;
            }
            else
            {
                m_currentTimeSeconds -= currentSegment.durationSeconds;
                m_beginValue = currentSegment.targetValue;
                ++m_currentSegmentIndex;
            }
        }

        if (m_currentSegmentIndex >= m_segments.size())
            m_currentValue = m_segments.back().targetValue;
    }
};

}  // namespace hart
