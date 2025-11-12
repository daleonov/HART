#pragma once

#include <cmath>  // abs()
#include <memory>
#include <string>

#include "hart_audio_buffer.hpp"
#include "signals/hart_signals_all.hpp"
#include "hart_utils.hpp"

namespace hart
{

template<typename SampleType>
class Matcher
{
public:
    virtual void prepare (double sampleRateHz, size_t numOutputChannels, size_t maxBlockSizeFrames) = 0;
    virtual bool match (const AudioBuffer<SampleType>& observedAudio) = 0;
    virtual bool canOperatePerBlock() = 0;
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
    EqualsTo (SignalType&& referenceSignal, SampleType epsilon = (SampleType) 1e-5):
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
        m_referenceSignal->renderNextBlock (referenceAudio);

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

    bool notEqual (SampleType x, SampleType y)
    {
        return std::abs (x - y) > m_epsilon;
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

template<typename SampleType>
class PeaksBelow:
    public Matcher<SampleType>
{
public:
    PeaksBelow (SampleType tresholdDb):
        m_tresholdDb (tresholdDb),
        m_tresholdLinear (decibelsToRatio (tresholdDb))
    {
    }

    void prepare (double /*sampleRateHz*/, size_t /* numOutputChannels */, size_t /* maxBlockSizeFrames */) override {}

    bool match (const AudioBuffer<SampleType>& observedAudio) override
    {
        for (size_t channel = 0; channel < observedAudio.getNumChannels(); ++channel)
            for (size_t frame = 0; frame < observedAudio.getNumFrames(); ++frame)
                if (std::abs (observedAudio[channel][frame]) > m_tresholdLinear)
                    return false;

        return true;
    }

    bool canOperatePerBlock() override
    {
        return true;
    }

    void reset() override {}

    virtual std::unique_ptr<Matcher<SampleType>> copy() const override
    {
        return std::make_unique<PeaksBelow<SampleType>> (*this);
    }

    std::string describe() const override
    {
        return std::string ("Peaks Below: ") + std::to_string (m_tresholdDb) + "dB";
    }

private:
    const SampleType m_tresholdDb;
    const SampleType m_tresholdLinear;
};

template <typename SampleType>
PeaksBelow<SampleType> peaksBelow (SampleType tresholdDb)
{
    return PeaksBelow<SampleType> (tresholdDb);
}

template<typename SampleType>
class PeaksAt:
    public Matcher<SampleType>
{
public:
    PeaksAt (SampleType targetDb, SampleType toleranceLinear = 1e-3):
        m_targetDb (targetDb),
        m_targetLinear (decibelsToRatio (targetDb)),
        m_toleranceLinear (toleranceLinear)
    {
    }

    void prepare (double /*sampleRateHz*/, size_t /* numOutputChannels */, size_t /* maxBlockSizeFrames */) override {}

    bool match (const AudioBuffer<SampleType>& observedAudio) override
    {
        SampleType observedPeakLinear = 0;

        for (size_t channel = 0; channel < observedAudio.getNumChannels(); ++channel)
            for (size_t frame = 0; frame < observedAudio.getNumFrames(); ++frame)
                observedPeakLinear = std::max (observedPeakLinear, std::abs (observedAudio[channel][frame]));

        return std::abs (observedPeakLinear - m_targetLinear) < m_toleranceLinear;
    }

    bool canOperatePerBlock() override
    {
        return false;
    }

    void reset() override {}

    virtual std::unique_ptr<Matcher<SampleType>> copy() const override
    {
        return std::make_unique<PeaksAt<SampleType>> (*this);
    }

    std::string describe() const override
    {
        return std::string ("PeaksAt (") + std::to_string (m_targetDb) + ")";
    }

private:
    const SampleType m_targetDb;
    const SampleType m_targetLinear;
    const SampleType m_toleranceLinear;
};

template <typename SampleType>
PeaksAt<SampleType> peaksAt (SampleType tresholdDb)
{
    return PeaksAt<SampleType> (tresholdDb);
}

}  // namespace hart
