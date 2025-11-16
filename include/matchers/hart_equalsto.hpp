#pragma once

#include <cmath>  // abs()

#include "matchers/hart_matcher.hpp"
#include "signals/hart_signal.hpp"

namespace hart
{

/// @brief Checks whether the audio is identical to some signal
/// @details Holds an internal Signal instance, and gets it to generate reference audio as
/// the matcher receives new audio blocks through match(). Reports mismatch if even
/// one of the frames in the audio does not match the reference signal within tolerance
/// specified in @c epsilon during its instantiation.
/// @ingroup Matchers
template<typename SampleType>
class EqualsTo:
    public Matcher<SampleType>
{
public:
    /// @brief Creates a matcher for a specific signal
    /// details The reference signal can be something simple like a @ref SineWave, or more
    /// complex signal with DSP effects chain and automation envelopes.
    /// @note Tip: To compare audio to a pre-recorded wav file, you can use @ref WavFile.
    /// @param referenceSignal Signal to compare the incoming audio against
    /// @param toleranceLinear Absolute tolerance for comparing frames, in linear domain (not decibels)
    template <typename SignalType>
    EqualsTo (const SignalType& referenceSignal, double toleranceLinear = (SampleType) 1e-5):
        m_referenceSignal (referenceSignal.copy()),
        m_toleranceLinear ((SampleType) toleranceLinear)
    {
        using DecayedType = typename std::decay<SignalType>::type;
        static_assert (
            std::is_base_of<Signal<SampleType>, DecayedType>::value,
            "SignalType must be a hart::Signal subclass"
            );
    }

    EqualsTo (EqualsTo&& other) noexcept:
        m_referenceSignal (std::move (other.m_referenceSignal)),
        m_toleranceLinear (other.m_toleranceLinear)
    {
    }

    EqualsTo (const EqualsTo& other):
        m_referenceSignal (other.m_referenceSignal != nullptr ? other.m_referenceSignal->copy() : nullptr),
        m_toleranceLinear (other.m_toleranceLinear)
    {
    }

    void prepare (double sampleRateHz, size_t numChannels, size_t maxBlockSizeFrames) override
    {
        m_referenceSignal->prepareWithDSPChain (sampleRateHz, numChannels, maxBlockSizeFrames);
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

    std::string describe() const override
    {
        return {};
    }

    void represent (std::ostream& stream) const
    {
        stream << "EqualsTo (" << *m_referenceSignal << ')';
    }

    HART_MATCHER_DEFINE_COPY_AND_MOVE (EqualsTo);

private:
    std::unique_ptr<Signal<SampleType>> m_referenceSignal;
    const SampleType m_toleranceLinear;

    inline bool notEqual (SampleType x, SampleType y)
    {
        return std::abs (x - y) > m_toleranceLinear;
    }
};

}  // namespace hart
