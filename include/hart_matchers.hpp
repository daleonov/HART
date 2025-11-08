#pragma once

#include <cmath>  // abs()
#include <memory>
#include <string>

#include "hart_audio_buffer.hpp"
#include "hart_signal.hpp"

namespace hart
{

template<typename SampleType>
class Matcher
{
public:
    virtual void prepare (double sampleRateHz, size_t numOutputChannels, size_t maxBlockSizeFrames) = 0;
    virtual bool match (const AudioBuffer<SampleType>& observedAudio) = 0;
    virtual void reset() = 0;
    virtual std::unique_ptr<Matcher<SampleType>> copy() const = 0;
    virtual std::string describe() const = 0;
};

template<typename SampleType>
class EqualsTo:
    public Matcher<SampleType>
{
public:
    template <typename SignalType>
    EqualsTo (SignalType&& referenceSignal, SampleType epsilon = (SampleType) 1e-6):
        m_referenceSignal (std::make_unique<std::decay_t<SignalType>> (std::forward<SignalType> (referenceSignal))),
        m_epsilon (epsilon)
    {
        static_assert (std::is_base_of_v<Signal<SampleType>, std::decay_t<SignalType>>, "SignalType must be a hart::Signal subclass");
    }

    EqualsTo (EqualsTo&& other):
        m_referenceSignal (std::move (other.m_referenceSignal)),
        m_epsilon (other.m_epsilon)
    {
    }

    EqualsTo (const EqualsTo& other):
        m_referenceSignal (other.m_referenceSignal != nullptr ? other.m_referenceSignal->copy() : nullptr),
        m_epsilon (other.m_epsilon)
    {
    }

    void prepare (double sampleRateHz, size_t numOutputChannels, size_t maxBlockSizeFrames) override
    {
        m_referenceSignal->prepare (sampleRateHz, numOutputChannels, maxBlockSizeFrames);
    }

    bool match (const AudioBuffer<SampleType>& observedAudio) override
    {
        auto referenceAudio = AudioBuffer<SampleType>::emptyLike (observedAudio);
        m_referenceSignal->renderNextBlock (referenceAudio.getArrayOfWritePointers(), referenceAudio.getNumFrames());

        for (int channel = 0; channel < referenceAudio.getNumChannels(); ++channel)
            for (int frame = 0; frame < referenceAudio.getNumFrames(); ++frame)
                if (notEqual (observedAudio[channel][frame], referenceAudio[channel][frame]))
                    return false;

        return true;
    }

    void reset() override
    {
        m_referenceSignal->reset();
    }

    virtual std::unique_ptr<Matcher<SampleType>> copy() const override
    {
        return std::make_unique<EqualsTo<SampleType>> (*this);
    }

    std::string describe() const override
    {
        return "Equals To: " + m_referenceSignal->describe();
    }

private:
    std::unique_ptr<Signal<SampleType>> m_referenceSignal;
    const SampleType m_epsilon;

    static bool notEqual (SampleType x, SampleType y)
    {
        return std::abs (x - y) > (SampleType) 1e-8;
    }
};

template <typename SignalType>
EqualsTo<typename SignalType::m_SampleType> equalsTo (SignalType&& signal)
{
    static_assert (std::is_base_of_v<Signal<typename SignalType::m_SampleType>, std::decay_t<SignalType>>, "SignalType must be a hart::Signal subclass");
    return EqualsTo<typename SignalType::m_SampleType> (std::forward<SignalType> (signal));
}

template <typename SignalType>
EqualsTo<typename SignalType::m_SampleType> equalsTo (const SignalType& signal)
{
    static_assert (std::is_base_of_v<Signal<typename SignalType::m_SampleType>, std::decay_t<SignalType>>, "SignalType must be a hart::Signal subclass");
    return EqualsTo<typename SignalType::m_SampleType> (signal);
}

}  // namespace hart
