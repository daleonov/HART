#pragma once

#include <cmath>  // abs()
#include <iomanip>
#include <sstream>

#include "hart_cliconfig.hpp"
#include "matchers/hart_matcher.hpp"
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
        {
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
        const int linDecimals = CLIConfig::get().getLinDecimals();
        const int dbDecimals = CLIConfig::get().getDbDecimals();
        const SampleType m_differenceLinear = std::abs (m_failedExpectedValue - m_failedObservedValue);

        std::stringstream stream;
        stream << std::fixed << std::setprecision (linDecimals)
            << "Expected sample value: " << m_failedExpectedValue
            << std::setprecision (dbDecimals)
            << " (" << ratioToDecibels (m_failedExpectedValue) << " dB)"
            <<  std::setprecision (linDecimals)
            << ", difference: " << m_differenceLinear
            << std::setprecision (dbDecimals)
            << " (" << ratioToDecibels (m_differenceLinear) << " dB)";

        MatcherFailureDetails details;
        details.frame = m_failedFrame;
        details.channel = m_failedChannel;
        details.description = std::move (stream.str());
        return details;
    }

    void represent (std::ostream& stream) const
    {
        stream << "EqualsTo (" << *m_referenceSignal
            << std::fixed << std::setprecision (CLIConfig::get().getLinDecimals())
            << ", " << m_toleranceLinear << ')';
    }

    HART_MATCHER_DEFINE_COPY_AND_MOVE (EqualsTo);

private:
    std::unique_ptr<Signal<SampleType>> m_referenceSignal;
    const SampleType m_toleranceLinear;

    size_t m_failedFrame;
    int m_failedChannel;
    SampleType m_failedObservedValue;
    SampleType m_failedExpectedValue;

    inline bool notEqual (SampleType x, SampleType y)
    {
        return std::abs (x - y) > m_toleranceLinear;
    }
};

}  // namespace hart
