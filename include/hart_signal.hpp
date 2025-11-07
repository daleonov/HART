#pragma once

#include <memory>

namespace hart {

template<typename SampleType>
class Signal
{
public:
    Signal (double durationSeconds = 0.0):
        m_durationSeconds (durationSeconds)
    {
    }

    virtual ~Signal() = default;

    virtual std::unique_ptr<Signal<SampleType>> copy() const
    {
        return std::make_unique<Signal<SampleType>> (*this);
    }

protected:
    const double m_durationSeconds;
    size_t m_durationFrames = 0;
    size_t m_blockSizeFrames = 0;
    double m_sampleRateHz = 44100.0;
};

template<typename SampleType>
class Silence:
    public Signal<SampleType>
{
public:
    Silence (double durationSeconds):
        Signal (durationSeconds)
    {
    }

    std::unique_ptr<Signal<SampleType>> copy() const override
    {
        return std::make_unique<Silence<SampleType>> (*this);
    }
};

}  // namespace hart
