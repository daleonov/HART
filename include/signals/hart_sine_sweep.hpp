#pragma once

#include <cmath>

#include "hart_exceptions.hpp"
#include "hart_precision.hpp"
#include "signals/hart_signal.hpp"
#include "hart_utils.hpp"

namespace hart {

/// @brief Produces a sine sweep
/// @details Outputs a signal at unity gain (-1.0..+1.0), linear or log sweep, up or down.
/// Tip: If you want to get an low-high-low or a high-low-high sweep, set `loop` to Loop::yes,
/// and duration of host's signal (see @ref AudioTestBuilder::withDuration())
/// to 2x ```durationSeconds``` of this signal.
/// @ingroup Signals
template<typename SampleType>
class SineSweep : public Signal<SampleType>
{
public:
    /// @brief Determines what to do after frequency sweep is done
    enum class Loop
    {
        no,  ///< Stop after finishing one sweep
        yes  ///< Keep on sweeping back and forth
    };

    /// @brief Determines how to change the frequency
    enum class SweepType
    {
        linear,  ///< Linear sweep, for a white noise-like spectrum
        log   ///< Logarithmic sweep, for a pink noise-like spectrum
    };

    /// @brief Creates a sine sweep signal
    /// @param durationSeconds Duration of sine sweep
    /// @param startFrequencyHz Start frequency of the sine sweep
    /// @param endFrequencyHz End frequency of the sine sweep
    /// @param type Linear or Log frequency sweep, see @ref SweepType
    /// @param loop If Loop::no is selected, the Signal will produce silence after duration;
    /// if Loop::yes is selected, the signal will keep on going back and forth between
    /// start and end frequencies (up and down) indefinitely.
    /// @param initialPhaseRadians Initial phase of the signal
    SineSweep (
    	double durationSeconds = 1.0,
        double startFrequencyHz = 20.0,
        double endFrequencyHz = 20.0e3,
        SweepType type = SweepType::log,
        Loop loop = Loop::no,
        double initialPhaseRadians = 0.0
        ):
        m_durationSeconds (durationSeconds),
        m_startFrequencyHz (startFrequencyHz),
        m_endFrequencyHz (endFrequencyHz),
        m_type (type),
        m_loop (loop),
        m_initialPhaseRadians (wrapPhase (initialPhaseRadians)),
        m_currentPhaseRadians (m_initialPhaseRadians),
        m_generateSilence (floatsEqual (durationSeconds, 0.0)),
        m_isFixedFrequency (floatsEqual (m_startFrequencyHz, m_endFrequencyHz)),
        m_frequencyRatio (m_endFrequencyHz / m_startFrequencyHz)
    {
        if (durationSeconds < 0)
            HART_THROW_OR_RETURN_VOID (hart::ValueError, "Duration cannot be negative");

        if (startFrequencyHz <= 0 || endFrequencyHz <= 0)
            HART_THROW_OR_RETURN_VOID (hart::ValueError, "Frequencies must be positive");
    }

    /// @brief Returns a new SineSweep instance with specified duration
    /// @details Handy if you want to skip specifying some arguments in the constructor
    /// @param durationSeconds Duration of sine sweep
    /// @return A new SineSweep instance with a specified parameter
    SineSweep withDuration (double durationSeconds)
    {
        return SineSweep (durationSeconds, m_startFrequencyHz, m_endFrequencyHz, m_type, m_loop, m_initialPhaseRadians);
    }

    /// @brief Returns a new SineSweep instance with specified start frequency
    /// @details Handy if you want to skip specifying some arguments in the constructor
    /// @param startFrequencyHz Start frequency of the sine sweep
    /// @return A new SineSweep instance with a specified parameter
    SineSweep withStartFrequency (double startFrequencyHz)
    {
        return SineSweep (m_durationSeconds, startFrequencyHz, m_endFrequencyHz, m_type, m_loop, m_initialPhaseRadians);
    }

    /// @brief Returns a new SineSweep instance with specified end frequency
    /// @details Handy if you want to skip specifying some arguments in the constructor
    /// @param endFrequencyHz End frequency of the sine sweep
    /// @return A new SineSweep instance with a specified parameter
    SineSweep withEndFrequency (double endFrequencyHz)
    {
        return SineSweep (m_durationSeconds, m_startFrequencyHz, endFrequencyHz, m_type, m_loop, m_initialPhaseRadians);
    }

    /// @brief Returns a new SineSweep instance with specified sweep type
    /// @details Handy if you want to skip specifying some arguments in the constructor
    /// @param type Linear or Log frequency sweep, see @ref SweepType
    /// @return A new SineSweep instance with a specified parameter
    SineSweep withType (SweepType type)
    {
        return SineSweep (m_durationSeconds, m_startFrequencyHz, m_endFrequencyHz, type, m_loop, m_initialPhaseRadians);
    }

    /// @brief Returns a new SineSweep instance with specified loop preference
    /// @details Handy if you want to skip specifying some arguments in the constructor
    /// @param loop If Loop::no is selected, the Signal will produce silence after duration;
    /// if Loop::yes is selected, the signal will keep on going back and forth between
    /// start and end frequencies (up and down) indefinitely.
    /// @return A new SineSweep instance with a specified parameter
    SineSweep withLoop (Loop loop)
    {
        return SineSweep (m_durationSeconds, m_startFrequencyHz, m_endFrequencyHz, m_type, loop, m_initialPhaseRadians);
    }

    /// @brief Returns a new SineSweep instance with specified initial phase
    /// @details Handy if you want to skip specifying some arguments in the constructor
    /// @param initialPhaseRadians Initial phase of the signal
    /// @return A new SineSweep instance with a specified parameter
    SineSweep withPhase (double initialPhaseRadians)
    {
        return SineSweep (m_durationSeconds, m_startFrequencyHz, m_endFrequencyHz, m_type, m_loop, initialPhaseRadians);
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
        m_currentPhaseRadians = m_initialPhaseRadians;
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
    const double m_initialPhaseRadians;
    double m_currentPhaseRadians;
    bool m_generateSilence;
    const bool m_isFixedFrequency;
    const double m_frequencyRatio;
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
