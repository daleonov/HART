#pragma once

#include <cmath>  // sin()
#include <memory>
#include <string>

namespace hart {

template<typename SampleType>
class Signal
{
public:
    virtual ~Signal() = default;

    virtual void setNumChannels (size_t numChannels)
    {
        m_numChannels = numChannels;
    }

    virtual int getNumChannels() const
    {
        return m_numChannels;
    }

    virtual void prepare (double sampleRateHz, size_t numOutputChannels, size_t maxBlockSizeFrames) = 0;
    virtual void renderNextBlock (SampleType* const* outputs, size_t numFrames) = 0;
    virtual void reset() = 0;
    virtual std::unique_ptr<Signal<SampleType>> copy() const = 0;
    virtual std::string describe() const = 0;
    
    using m_SampleType = SampleType;

protected:
    size_t m_numChannels = 1;
};

template<typename SampleType>
class Silence:
    public Signal<SampleType>
{
public:
    void prepare (double /* sampleRateHz */, size_t numOutputChannels, size_t /* maxBlockSizeFrames */) override
    {
        setNumChannels (numOutputChannels);
    }

    void renderNextBlock (SampleType* const* outputs, size_t numFrames) override
    {
        for (size_t channel = 0; channel < this->m_numChannels; ++channel)
            for (size_t frame = 0; frame < numFrames; ++frame)
                outputs[channel][frame] = (SampleType) 0;
    }

    void reset() override {}

    std::unique_ptr<Signal<SampleType>> copy() const override
    {
        return std::make_unique<Silence<SampleType>> (*this);
    }

    std::string describe() const override
    {
        return "Silence";
    }

};

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
        }

    void prepare (double sampleRateHz, size_t numOutputChannels, size_t /*maxBlockSizeFrames*/) override
    {
        m_sampleRateHz = sampleRateHz;
        this->setNumChannels (numOutputChannels);
    }

    void renderNextBlock (SampleType* const* outputs, size_t numFrames) override
    {
        const double phaseIncrement = m_twoPi * m_frequencyHz / m_sampleRateHz;

        for (size_t frame = 0; frame < numFrames; ++frame)
        {
            SampleType value = static_cast<SampleType> (std::sin (m_phaseRadians));

            for (size_t channel = 0; channel < this->m_numChannels; ++channel)
                outputs[channel][frame] = value;

            m_phaseRadians += phaseIncrement;
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
};

}  // namespace hart
