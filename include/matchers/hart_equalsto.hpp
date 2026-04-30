#pragma once

#include <cmath>  // abs()
#include <iomanip>
#include <sstream>

#include "hart_exceptions.hpp"
#include "matchers/hart_matcher.hpp"
#include "hart_precision.hpp"
#include "signals/hart_signal.hpp"
#include "hart_utils.hpp"  // floatsEqual(), ratioToDecibels()

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
        m_maxBlockSizeFrames = maxBlockSizeFrames;
        m_referenceOutputAudio = AudioBuffer<SampleType> (numChannels, maxBlockSizeFrames, sampleRateHz);
        m_referenceSignal->prepareWithDSPChain (sampleRateHz, numChannels, maxBlockSizeFrames);
    }

    bool match (const AudioBuffer<SampleType>& /* inputAudio */, const AudioBuffer<SampleType>& observedOutputAudio) override
    {
        if (observedOutputAudio.getNumFrames() <= m_maxBlockSizeFrames)
        {
            // Usual block-wise rendering

            if (m_referenceOutputAudio.getNumFrames() != observedOutputAudio.getNumFrames())
                m_referenceOutputAudio.setNumFrames (observedOutputAudio.getNumFrames());  // To partial block and back

            m_referenceSignal->renderNextBlockWithDSPChain (m_referenceOutputAudio);
        }
        else
        {
            // Matcher was given a full piece of audio, instead of block-by-block rendering.
            // HART guarantees to the Signal instances that the renderNextBlock() will be called
            // with audio buffer sizes that are in line with max block size set in prepare().
            // Since this matcher is a "host" for the reference signal, it has to obey those
            // rules as well, by slicing the full audio buffer into properly sized blocks.

            AudioBuffer<SampleType> fullOutputBuffer (observedOutputAudio.getNumChannels(), 0, observedOutputAudio.getSampleRateHz());
            const size_t totalDurationFrames = observedOutputAudio.getNumFrames();
            size_t offsetFrames = 0;

            while (offsetFrames < totalDurationFrames)
            {
                const size_t blockSizeFrames = std::min (m_maxBlockSizeFrames, totalDurationFrames - offsetFrames);

                if (m_referenceOutputAudio.getNumFrames() != blockSizeFrames)
                    m_referenceOutputAudio.setNumFrames (blockSizeFrames);  // To partial block and back

                m_referenceSignal->renderNextBlockWithDSPChain (m_referenceOutputAudio);
                fullOutputBuffer.appendFrom (m_referenceOutputAudio);

                offsetFrames += blockSizeFrames;
            }

            m_referenceOutputAudio = std::move (fullOutputBuffer);
        }

        // Sanity checks
        hassert (m_referenceOutputAudio.getNumFrames() == observedOutputAudio.getNumFrames());
        hassert (m_referenceOutputAudio.getNumChannels() == observedOutputAudio.getNumChannels());
        hassert (floatsEqual (m_referenceOutputAudio.getSampleRateHz(), observedOutputAudio.getSampleRateHz()));

        for (size_t channel = 0; channel < m_referenceOutputAudio.getNumChannels(); ++channel)
        {
            if (! this->appliesToChannel (channel))
                continue;

            for (size_t frame = 0; frame < m_referenceOutputAudio.getNumFrames(); ++frame)
            {
                if (notEqual (observedOutputAudio[channel][frame], m_referenceOutputAudio[channel][frame]))
                {
                    m_failedFrame = frame;
                    m_failedChannel = (int) channel;
                    m_failedObservedValue = observedOutputAudio[channel][frame];
                    m_failedExpectedValue = m_referenceOutputAudio[channel][frame];
                    return false;
                }
            }
        }

        return true;
    }

    bool canOperatePerBlock() const override
    {
        return true;
    }

    bool supportsChannelLayout (size_t /* numInputChannels */, size_t numOutputChannels) const
    {
        return m_referenceSignal->supportsNumChannelsWithDSPChain (numOutputChannels);
    }

    bool supportsSampleRate (double sampleRateHz) const
    {
        return m_referenceSignal->supportsSampleRateWithDSPChain (sampleRateHz);
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

    size_t m_maxBlockSizeFrames = 0;
    AudioBuffer<SampleType> m_referenceOutputAudio;

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
