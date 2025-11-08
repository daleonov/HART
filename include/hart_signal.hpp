#pragma once

#include <memory>

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

    virtual int getNumChannels()
    {
        return m_numChannels;
    }

    virtual void renderNextBlock (SampleType* const* outputs, size_t numFrames) = 0;
    virtual void reset() = 0;
    virtual std::unique_ptr<Signal<SampleType>> copy() const = 0;

protected:
    size_t m_numChannels = 1;
};

template<typename SampleType>
class Silence:
    public Signal<SampleType>
{
public:
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
};

}  // namespace hart
