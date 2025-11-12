#pragma once

#include <algorithm>
#include <cmath>  // sin()
#include <cstdint>
#include <memory>
#include <random>
#include <string>
#include <vector>

#include "hart_audio_buffer.hpp"
#include "dsp/hart_dsp.hpp"

namespace hart {

template<typename SampleType>
class Signal
{
public:
    Signal() = default;

    Signal (const Signal& other):
        m_numChannels(other.m_numChannels)
    {
        if (other.dspChain.size() == 0)
            return;

        dspChain.reserve (dspChain.size());

        for (auto& dsp : other.dspChain)
            dspChain.push_back (dsp->copy());
    }

    Signal (Signal&& other) noexcept:
        m_numChannels (other.m_numChannels),
        dspChain (std::move (other.dspChain))
    {
        other.m_numChannels = 0;
    }

    virtual ~Signal() = default;

    Signal& operator= (const Signal& other)
    {
        if (this == &other)
            return *this;

        m_numChannels = other.m_numChannels;
        dspChain.reset();

        if (other.dspChain.size() == 0)
            return *this;

        for (auto& dsp : other.dspChain)
            dspChain.push_back (dsp->copy());
        
        return *this;
    }

    Signal& operator= (Signal&& other) noexcept
    {
        if (this == &other)
            return *this;

        m_numChannels = other.m_numChannels;
        dspChain = std::move (other.dspChain);
        other.m_numChannels = 0;

        return *this;
    }

    virtual void setNumChannels (size_t numChannels)
    {
        m_numChannels = numChannels;
    }

    virtual int getNumChannels() const
    {
        return m_numChannels;
    }

    virtual void prepare (double sampleRateHz, size_t numOutputChannels, size_t maxBlockSizeFrames) = 0;
    virtual void renderNextBlock (AudioBuffer<SampleType>& output) = 0;
    virtual void reset() = 0;
    virtual std::unique_ptr<Signal<SampleType>> copy() const = 0;
    virtual std::string describe() const = 0;

    Signal& followedBy (const DSP<SampleType>& dsp)
    {
        dspChain.emplace_back (dsp.copy());
        return *this;
    }

    template <typename DerivedDSP, typename = std::enable_if_t<std::is_base_of_v<DSP<SampleType>, std::decay_t<DerivedDSP>>>>
    Signal& followedBy (DerivedDSP&& dsp)
    {
        dspChain.emplace_back (
            std::make_unique<std::decay_t<DerivedDSP>> (std::forward<DerivedDSP> (dsp))
        );
        return *this;
    }

    using m_SampleType = SampleType;

protected:
    size_t m_numChannels = 1;
    std::vector<std::unique_ptr<DSP<SampleType>>> dspChain;
};

template<typename SampleType>
Signal<SampleType>& operator>> (Signal<SampleType>& signal, DSP<SampleType>&& dsp)
{
    return signal.followedBy (std::move (dsp));
}

template<typename SampleType>
Signal<SampleType>& operator>> (Signal<SampleType>& signal, const DSP<SampleType>& dsp)
{
    return signal.followedBy (dsp);;
}

template<typename SampleType>
Signal<SampleType>&& operator>> (Signal<SampleType>&& signal, const DSP<SampleType>& dsp)
{
    return std::move (signal.followedBy (dsp));
}

}  // namespace hart
