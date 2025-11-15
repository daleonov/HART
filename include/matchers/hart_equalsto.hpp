#pragma once

#include <cmath>  // abs()

#include "matchers/hart_matcher.hpp"
#include "signals/hart_signals_all.hpp"

namespace hart
{

template<typename SampleType>
class EqualsTo:
    public Matcher<SampleType>
{
public:
    template <typename SignalType>
    EqualsTo (const SignalType& referenceSignal, SampleType epsilon = (SampleType) 1e-5):
        m_referenceSignal (referenceSignal.copy()),
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
        m_referenceSignal->prepareWithDSPChain (sampleRateHz, numOutputChannels, maxBlockSizeFrames);
    }

    bool match (const AudioBuffer<SampleType>& observedAudio) override
    {
        auto referenceAudio = AudioBuffer<SampleType>::emptyLike (observedAudio);
        m_referenceSignal->renderNextBlockWithDSPChain (referenceAudio);

        for (size_t channel = 0; channel < referenceAudio.getNumChannels(); ++channel)
            for (size_t frame = 0; frame < referenceAudio.getNumFrames(); ++frame)
                if (notEqual (observedAudio[channel][frame], referenceAudio[channel][frame]))
                    return false;

        return true;
    }

    bool canOperatePerBlock() override
    {
        return true;
    }

    void reset() override
    {
        m_referenceSignal->resetWithDSPChain();
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

    bool notEqual (SampleType x, SampleType y)
    {
        return std::abs (x - y) > m_epsilon;
    }
};

}  // namespace hart
