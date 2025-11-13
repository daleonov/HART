#pragma once

#include <cmath>
#include <memory>
#include <string>

#include "hart_exceptions.hpp"
#include "signals/hart_signal.hpp"
#include "hart_utils.hpp"

namespace hart
{

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

    void prepare (double sampleRateHz, size_t numOutputChannels, size_t /*maxBlockSizeFrames*/) override
    {
        m_sampleRateHz = sampleRateHz;
        this->setNumChannels (numOutputChannels);
    }

    void renderNextBlock (AudioBuffer<SampleType>& output) override
    {
        if (output.getNumChannels() != this->getNumChannels())
            HART_THROW_OR_RETURN_VOID (ChannelLayoutError, std::string ("Signal was configured for a different channel number") + describe());

        const double phaseIncrement = m_twoPi * m_frequencyHz / m_sampleRateHz;

        for (size_t frame = 0; frame < output.getNumFrames(); ++frame)
        {
            SampleType value = static_cast<SampleType> (std::sin (m_phaseRadians));

            for (size_t channel = 0; channel < this->m_numChannels; ++channel)
                output[channel][frame] = value;

            m_phaseRadians += phaseIncrement;
            clampPhase();
        }
    }

    void reset() override
    {
        m_phaseRadians = m_initialPhaseRadians;
    }

    std::unique_ptr<Signal<SampleType>> copy() const override
    {
        return std::make_unique<SineWave<SampleType>> (*this);
    }

    std::string describe() const override
    {
        return std::string ("Sine Wave, frequency = ") + std::to_string (m_frequencyHz) + "Hz, phase = " + std::to_string(m_initialPhaseRadians) + " radians";
    }

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
