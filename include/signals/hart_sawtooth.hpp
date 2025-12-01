#pragma once

#include <cmath>

#include "hart_exceptions.hpp"
#include "hart_precision.hpp"
#include "signals/hart_signal.hpp"
#include "hart_utils.hpp"

namespace hart
{

/// @brief Produces a bandlimited sawtooth wave at fixed frequency
/// @details Outputs a signal peaking at unity gain (-1.0..+1.0). Rising edge, phase = 0 corresponds to value = -1.0.
/// Uses quadratic PolyBLEP for anti aliasing.
/// @ingroup Signals
template<typename SampleType>
class Sawtooth:
    public Signal<SampleType, Sawtooth<SampleType>>
{
public:
    /// @brief Creates a sawtooth signal instance
    /// @param frequencyHz Fixed frequency in Hz
    /// @param phaseRadians Initial phase in radians
    Sawtooth (double frequencyHz = 1000.0, double phaseRadians = 0.0):
        m_frequencyHz (frequencyHz),
        m_initialPhaseRadians (wrapPhase (phaseRadians)),
        m_phaseRadians (m_initialPhaseRadians)
    {
        if (frequencyHz <= 0.0)
            HART_THROW (hart::ValueError, "Invalid frequency value");
    }

    void prepare (double sampleRateHz, size_t /* numOutputChannels */, size_t /*maxBlockSizeFrames*/) override
    {
        m_sampleRateHz = sampleRateHz;
        m_phaseIncrementRadians = hart::twoPi * m_frequencyHz / m_sampleRateHz;
        m_ratio = m_frequencyHz / m_sampleRateHz;
        adjustMakeupGain();
    }

    void renderNextBlock (AudioBuffer<SampleType>& output) override
    {
        for (size_t frame = 0; frame < output.getNumFrames(); ++frame)
        {
            const double portion = m_phaseRadians / hart::twoPi;
            SampleType value = SampleType (2.0 * portion - 1.0);

            if (portion < m_ratio)
            {
                const double t = portion / m_ratio;
                value += SampleType ((t - 1.0) * (t - 1.0));
            }
            else if (portion > 1.0 - m_ratio)
            {
                const double t = (portion - 1.0) / m_ratio;
                value -= SampleType ((t + 1.0) * (t + 1.0));
            }

            value *= m_makeupGainLinear;

            for (size_t channel = 0; channel < output.getNumChannels(); ++channel)
                output[channel][frame] = value;

            m_phaseRadians = wrapPhase (m_phaseRadians + m_phaseIncrementRadians);
        }
    }

    void reset() override
    {
        m_phaseRadians = m_initialPhaseRadians;
    }

    void represent (std::ostream& stream) const override
    {
        stream << "Sawtooth ("
            << hzPrecision << m_frequencyHz << "_Hz, "
            << radPrecision << m_initialPhaseRadians << "_rad)";
    }

private:
    const double m_frequencyHz;
    const double m_initialPhaseRadians;
    double m_phaseRadians = 0.0;
    double m_sampleRateHz = 44100.0;
    double m_phaseIncrementRadians = hart::twoPi * m_frequencyHz / m_sampleRateHz;
    double m_ratio = m_frequencyHz / m_sampleRateHz;
    SampleType m_makeupGainLinear = (SampleType) 1;

    void adjustMakeupGain()
    {
        // TODO: Calculate the peak instead of measuring it
        const double cyclesToMeasure = 32.0;
        const size_t framesToMeasure = static_cast<size_t> (cyclesToMeasure * m_sampleRateHz / m_frequencyHz);
        const double originalPhaseRadians = m_phaseRadians;
        m_makeupGainLinear = (SampleType) 1;

        AudioBuffer<SampleType> buffer (1, framesToMeasure);
        renderNextBlock (buffer);

        SampleType peakLinear = (SampleType) 0;

        for (size_t i = 0; i < framesToMeasure; ++i)
            peakLinear = std::max (peakLinear, std::abs (buffer[0][i]));

        m_makeupGainLinear = SampleType ((peakLinear > (SampleType) 0) ?  SampleType (1) / peakLinear : (SampleType) 1);
        m_phaseRadians = originalPhaseRadians;      
    }
};

HART_SIGNAL_DECLARE_ALIASES_FOR (Sawtooth)

}  // namespace hart
