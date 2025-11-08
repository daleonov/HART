#pragma once

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

}  // namespace hart
