#pragma once

#include <cmath>  // abs()
#include <iomanip>
#include <sstream>

#include "matchers/hart_matcher.hpp"
#include "hart_precision.hpp"
#include "signals/hart_signal.hpp"
#include "hart_utils.hpp"

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
    public Matcher<SampleType, EqualsTo<SampleType>>
{
public:
    /// @brief Creates a matcher for a specific signal by transfering smart pointer
    /// details The reference signal can be something simple like a @ref SineWave, or more
    /// complex signal with DSP effects chain and automation envelopes.
    /// @note Tip: To compare audio to a pre-recorded wav file, you can use @ref WavFile.
    /// @param referenceSignal Signal to compare the incoming audio against
    /// @param toleranceLinear Absolute tolerance for comparing frames, in linear domain (not decibels)
    EqualsTo (std::unique_ptr<SignalBase<SampleType>>&& referenceSignal, double toleranceLinear = (SampleType) 1e-5):
        m_referenceSignal (std::move (referenceSignal)),
        m_toleranceLinear ((SampleType) toleranceLinear)
    {
    }
    
    /// @brief Creates a matcher for a specific signal by copying it
    /// details The reference signal can be something simple like a @ref SineWave, or more
    /// complex signal with DSP effects chain and automation envelopes.
    /// @note Tip: To compare audio to a pre-recorded wav file, you can use @ref WavFile.
    /// @param referenceSignal Signal to compare the incoming audio against
    /// @param toleranceLinear Absolute tolerance for comparing frames, in linear domain (not decibels)
    EqualsTo (const SignalBase<SampleType>& referenceSignal, double toleranceLinear = (SampleType) 1e-5):
        EqualsTo (referenceSignal.copy(), toleranceLinear)
    {
    }

    /// @brief Creates a matcher for a specific signal by moving it
    /// details The reference signal can be something simple like a @ref SineWave, or more
    /// complex signal with DSP effects chain and automation envelopes.
    /// @note Tip: To compare audio to a pre-recorded wav file, you can use @ref WavFile.
    /// @param referenceSignal Signal to compare the incoming audio against
    /// @param toleranceLinear Absolute tolerance for comparing frames, in linear domain (not decibels)
    EqualsTo (SignalBase<SampleType>&& referenceSignal, double toleranceLinear = (SampleType) 1e-5):
        EqualsTo (referenceSignal.move(), toleranceLinear)
    {
    }

    EqualsTo (EqualsTo&& other) noexcept:
        Matcher<SampleType, EqualsTo<SampleType>> (std::move (other)),
        m_referenceSignal (std::move (other.m_referenceSignal)),
        m_toleranceLinear (other.m_toleranceLinear)
    {
    }

    EqualsTo (const EqualsTo& other):
        Matcher<SampleType, EqualsTo<SampleType>> (other),
        m_referenceSignal (other.m_referenceSignal != nullptr ? other.m_referenceSignal->copy() : nullptr),
        m_toleranceLinear (other.m_toleranceLinear)
    {
    }

    EqualsTo& operator= (const EqualsTo& other)
    {
        if (this == &other)
            return *this;

        Matcher<SampleType, EqualsTo<SampleType>>::operator= (other);

        m_referenceSignal = other.m_referenceSignal != nullptr ? other.m_referenceSignal->copy() : nullptr;
        m_toleranceLinear = other.m_toleranceLinear;

        return *this;
    }

    EqualsTo& operator= (EqualsTo&& other) noexcept
    {
        if (this == &other)
            return *this;

        // Move base first
        Matcher<SampleType, EqualsTo<SampleType>>::operator=(std::move(other));

        // Then move our members
        m_referenceSignal = std::move(other.m_referenceSignal);
        m_toleranceLinear = other.m_toleranceLinear;

        return *this;
    }

    ~EqualsTo() override = default;

    void prepare (double sampleRateHz, size_t numChannels, size_t maxBlockSizeFrames) override
    {
        m_referenceSignal->prepareWithDSPChain (sampleRateHz, numChannels, maxBlockSizeFrames);
    }

    bool match (const AudioBuffer<SampleType>& observedAudio) override
    {
        auto referenceAudio = AudioBuffer<SampleType>::emptyLike (observedAudio);
        m_referenceSignal->renderNextBlockWithDSPChain (referenceAudio);

        for (size_t channel = 0; channel < referenceAudio.getNumChannels(); ++channel)
        {
            if (! this->appliesToChannel (channel))
                continue;

            for (size_t frame = 0; frame < referenceAudio.getNumFrames(); ++frame)
            {
                if (notEqual (observedAudio[channel][frame], referenceAudio[channel][frame]))
                {
                    m_failedFrame = frame;
                    m_failedChannel = (int) channel;
                    m_failedObservedValue = observedAudio[channel][frame];
                    m_failedExpectedValue = referenceAudio[channel][frame];
                    return false;
                }
            }
        }

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

    virtual MatcherFailureDetails getFailureDetails() const override
    {
        const SampleType m_differenceLinear = std::abs (m_failedExpectedValue - m_failedObservedValue);
        std::stringstream stream;
        stream << linPrecision << "Expected sample value: " << m_failedExpectedValue
            << dbPrecision << " (" << ratioToDecibels (m_failedExpectedValue) << " dB)"
            << linPrecision << ", difference: " << m_differenceLinear
            << dbPrecision << " (" << ratioToDecibels (m_differenceLinear) << " dB)";

        MatcherFailureDetails details;
        details.frame = m_failedFrame;
        details.channel = m_failedChannel;
        details.description = stream.str();
        return details;
    }

    void represent (std::ostream& stream) const override
    {
        stream << "EqualsTo (" << *m_referenceSignal
            << linPrecision << ", " << m_toleranceLinear << ')';
    }

private:
    std::unique_ptr<SignalBase<SampleType>> m_referenceSignal;
    const SampleType m_toleranceLinear;

    size_t m_failedFrame = 0;
    size_t m_failedChannel = 0;
    SampleType m_failedObservedValue = (SampleType) 0;
    SampleType m_failedExpectedValue = (SampleType) 0;

    inline bool notEqual (SampleType x, SampleType y)
    {
        return std::abs (x - y) > m_toleranceLinear;
    }
};

HART_MATCHER_DECLARE_ALIASES_FOR (EqualsTo)

}  // namespace hart
