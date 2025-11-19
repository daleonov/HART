#pragma once

#include <cmath>

#include "hart_exceptions.hpp"
#include "hart_precision.hpp"
#include "signals/hart_signal.hpp"
#include "hart_utils.hpp"

namespace hart {

/// @brief Sine sweep (linear or logarithmic)
/// @ingroup Signals
template<typename SampleType>
class SineSweep : public Signal<SampleType>
{
public:
    enum class Loop
    {
        no,
        yes
    };

    enum class SweepType
    {
        linear,
        log
    };

    SineSweep (
    	double durationSeconds = 1.0,
        double startFrequencyHz = 20.0,
        double endFrequencyHz = 20.0e3,
        SweepType type = SweepType::log,
        Loop loop = Loop::no
        ):
        m_durationSeconds (durationSeconds),
        m_startFrequencyHz (startFrequencyHz),
        m_endFrequencyHz (endFrequencyHz),
        m_type (type),
        m_loop (loop),
        m_generateSilence (floatsEqual (durationSeconds, 0.0)),
        m_isFixedFrequency (floatsEqual (m_startFrequencyHz, m_endFrequencyHz)),
        m_frequencyRatio (m_endFrequencyHz / m_startFrequencyHz)
    {
        if (durationSeconds < 0)
            HART_THROW_OR_RETURN_VOID (hart::ValueError, "Duration cannot be negative");

        if (startFrequencyHz <= 0 || endFrequencyHz <= 0)
            HART_THROW_OR_RETURN_VOID (hart::ValueError, "Frequencies must be positive");
    }

    SineSweep withDuration (double durationSeconds)
    {
        return SineSweep (durationSeconds, m_startFrequencyHz, m_endFrequencyHz, m_type, m_loop);
    }

    SineSweep withType (SweepType type)
    {
        return SineSweep (m_durationSeconds, m_startFrequencyHz, m_endFrequencyHz, type, m_loop);
    }

    SineSweep withLoop (Loop loop)
    {
        return SineSweep (m_durationSeconds, m_startFrequencyHz, m_endFrequencyHz, m_type, loop);
    }

    SineSweep withStartFrequency (double startFrequencyHz)
    {
        return SineSweep (m_durationSeconds, startFrequencyHz, m_endFrequencyHz, m_type, m_loop);
    }

    SineSweep withEndFrequency (double endFrequencyHz)
    {
        return SineSweep (m_durationSeconds, m_startFrequencyHz, endFrequencyHz, m_type, m_loop);
    }

    bool supportsNumChannels (size_t /*numChannels*/) const override { return true; }

    void prepare (double sampleRateHz, size_t /*numOutputChannels*/, size_t /*maxBlockSizeFrames*/) override
    {
        m_sampleRateHz = sampleRateHz;
        m_durationFrames = roundToSizeT (m_durationSeconds * m_sampleRateHz);
    }

    void renderNextBlock (AudioBuffer<SampleType>& output) override
    {
        if (m_generateSilence)
        {
            fillWithSilence (output);
            return;
        }

        for (size_t frame = 0; frame < output.getNumFrames(); ++frame)
        {
            const SampleType value = std::sin (m_currentPhaseRadians);

            for (size_t channel = 0; channel < output.getNumChannels(); ++channel)
                output[channel][frame] = value;

            ++m_posFrames;

            if (m_posFrames == m_durationFrames)
            {
                if (m_loop == Loop::yes)
                {
                    // Reverse frequency direction
                    m_reverseFrequencyDirection = ! m_reverseFrequencyDirection;
                    m_posFrames = 0;
                }
                else
                {
                    // Stop generating the sine
                    fillWithSilence (output, frame + 1);
                    m_posFrames = 0;
                    m_generateSilence = true;
                    break;
                }
            }

            const double currentFrequencyHz = frequencyAtFrame (m_posFrames, m_reverseFrequencyDirection);
            m_currentPhaseRadians += hart::twoPi * currentFrequencyHz / m_sampleRateHz;
            m_currentPhaseRadians = wrapPhase (m_currentPhaseRadians);
        }
    }

    void reset() override
    {
        m_posFrames = 0;
        m_currentPhaseRadians = 0;
        m_generateSilence = floatsEqual (m_durationSeconds, 0.0);
        m_reverseFrequencyDirection = false;
    }

    void represent (std::ostream& stream) const override
    {
        stream << "SineSweep ("
            << secPrecision
            << m_durationSeconds << "_s, "
            << hzPrecision
            << m_startFrequencyHz << "_Hz, "
            << m_endFrequencyHz << "_Hz, "
            << (m_type == SweepType::linear ? ", SweepType::linear" : "SweepType::log")
            << (m_loop == Loop::yes ? ", Loop::yes)" : ", Loop::no)");
    }

    HART_SIGNAL_DEFINE_COPY_AND_MOVE (SineSweep);

private:
    const double m_durationSeconds;
    const double m_startFrequencyHz;
    const double m_endFrequencyHz;
    const SweepType m_type;
    const Loop m_loop;

    double m_sampleRateHz = 0.0;
    size_t m_durationFrames = 0;
    size_t m_posFrames = 0;
    bool m_generateSilence;
    const bool m_isFixedFrequency;
    const double m_frequencyRatio;

    double m_currentPhaseRadians = 0.0;
    bool m_reverseFrequencyDirection = false;

    void fillWithSilence (AudioBuffer<SampleType>& output, size_t startingFrame = 0)
    {
        if (startingFrame >= output.getNumFrames())
            return;

        for (size_t channel = 0; channel < output.getNumChannels(); ++channel)
            std::fill (output[channel] + startingFrame, output[channel] + output.getNumFrames(), (SampleType) 0);
    }

    double frequencyAtFrame (size_t offsetFrames, bool reverseFrequencyDirection) const
    {
        if (m_isFixedFrequency)
            return m_startFrequencyHz;

        hassert (offsetFrames < m_durationFrames);
        const double offsetSeconds = static_cast<double> (offsetFrames) / m_sampleRateHz;

        double portion = offsetSeconds / m_durationSeconds;
        portion = reverseFrequencyDirection ? 1.0 - portion : portion;

        if (m_type == SweepType::linear)
            return m_startFrequencyHz + (m_endFrequencyHz - m_startFrequencyHz) * portion;

        // SweepType::logaritmic
        return m_startFrequencyHz * std::pow (m_frequencyRatio, portion);
    }
};

} // namespace hart
