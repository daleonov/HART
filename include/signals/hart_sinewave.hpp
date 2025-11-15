#pragma once

#include <cmath>
#include <memory>
#include <string>

#include "hart_exceptions.hpp"
#include "signals/hart_signal.hpp"
#include "hart_utils.hpp"

namespace hart
{

/// @brief Produces a sine wave at fixed frequency
/// @details Outputs a signal at 0dB peak (-1.0..+1.0)
/// @ingroup Signals
template<typename SampleType>
class SineWave:
    public Signal<SampleType>
{
public:
    SineWave (double frequencyHz = (SampleType) 1000, double phaseRadians = (SampleType) 0):
        m_frequencyHz (frequencyHz),
        m_initialPhaseRadians (phaseRadians),
        m_phaseRadians (phaseRadians)
        {
            clampPhase();

            if (frequencyHz <= 0)
                HART_THROW (hart::ValueError, std::string ("Invalid frequency value for: ") + describe());
        }

    bool supportsNumChannels (size_t numChannels) const override { return true; };

    void prepare (double sampleRateHz, size_t /* numOutputChannels */, size_t /*maxBlockSizeFrames*/) override
    {
        m_sampleRateHz = sampleRateHz;
    }

    void renderNextBlock (AudioBuffer<SampleType>& output) override
    {
        const double phaseIncrement = m_twoPi * m_frequencyHz / m_sampleRateHz;

        for (size_t frame = 0; frame < output.getNumFrames(); ++frame)
        {
            SampleType value = static_cast<SampleType> (std::sin (m_phaseRadians));

            for (size_t channel = 0; channel < output.getNumChannels(); ++channel)
                output[channel][frame] = value;

            m_phaseRadians += phaseIncrement;
            clampPhase();
        }
    }

    void reset() override
    {
        m_phaseRadians = m_initialPhaseRadians;
    }

    std::string describe() const override
    {
        return std::string ("Sine Wave, frequency = ") + std::to_string (m_frequencyHz) + "Hz, phase = " + std::to_string(m_initialPhaseRadians) + " radians";
    }

    HART_SIGNAL_DEFINE_COPY_AND_MOVE (SineWave);

private:
    static constexpr double m_twoPi = 2.0 * 3.141592653589793;
    const double m_frequencyHz;
    const double m_initialPhaseRadians;
    double m_phaseRadians;
    double m_sampleRateHz;

    void clampPhase()
    {
        while (m_phaseRadians < 0)
            m_phaseRadians += hart::twoPi;

        if (m_phaseRadians > hart::twoPi)
            m_phaseRadians = std::fmod (m_phaseRadians, hart::twoPi);
    }
};

}  // namespace hart
