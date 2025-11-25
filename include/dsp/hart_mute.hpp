#pragma once

#include <algorithm>  // fill
#include <bitset>
#include <iomanip>  // hex

#include "hart_dsp.hpp"
#include "hart_exceptions.hpp"

namespace hart
{

/// @brief Mutes selected channels in the signal
/// @details Applies zero to specified channels
/// @ingroup DSP
template <typename SampleType>
class Mute : public hart::DSP<SampleType>
{
private:
    static constexpr size_t m_maxChannels = 64;

public:
    /// @brief Creates a Mute object that mutes all channels
    Mute():
        m_channelsToMute (~std::bitset<m_maxChannels>{})
    {
    }

    /// @brief Creates a Mute object from bitset mask
    /// @param channelsToMute Channels to mute (1 = muted, 0 = bypass)
    explicit Mute (std::bitset<m_maxChannels> channelsToMute)
        : m_channelsToMute (channelsToMute)
    {
    }

    /// @brief Creates a Mute object from channels to mute (0-based)
    /// @param channelsToMute List of channels to mute
    /// @see hart::Channel
    Mute (std::initializer_list<size_t> channelsToMute)
    {
        for (size_t channel : channelsToMute)
        {
            if (channel >= m_channelsToMute.size())
                HART_THROW_OR_RETURN_VOID (hart::ValueError, "Channel exceeds max number of channels");

            m_channelsToMute.set (channel);
        }
    }

    void prepare (double /*sampleRateHz*/, size_t numInputChannels, size_t numOutputChannels, size_t /*maxBlockSizeFrames*/) override
    {
        if (numInputChannels != numOutputChannels || numInputChannels > m_maxChannels)
            HART_THROW_OR_RETURN_VOID (hart::ChannelLayoutError, "Unsupported channel configuration");
    }

    void process (const AudioBuffer<SampleType>& input, AudioBuffer<SampleType>& output, const EnvelopeBuffers& /*envelopeBuffers*/) override
    {
        if (input.getNumChannels() != output.getNumChannels())
            HART_THROW_OR_RETURN_VOID (hart::ChannelLayoutError, "Channel number mismatch");

        const size_t numChannels = output.getNumChannels();
        const size_t numFrames = output.getNumFrames();

        for (size_t channel = 0; channel < numChannels; ++channel)
        {
            if (m_channelsToMute.test (channel))
                output.clear (channel, 0, numFrames);
            else
                output.copyFrom (channel, 0, input, channel, 0, numFrames);
        }
    }

    void reset() override {}

    void setValue (int /*id*/, double /*value*/) override {}

    double getValue (int /*id*/) const override { return 0.0; }

    bool supportsChannelLayout (size_t numInputChannels, size_t numOutputChannels) const override
    {
        return numInputChannels == numOutputChannels && numInputChannels <= m_maxChannels;
    }

    void represent (std::ostream& stream) const override
    {
        stream << "Mute (0b" << m_channelsToMute << ")";
    }

    bool supportsEnvelopeFor(int /*id*/) const override
    {
        return false;
    }

    HART_DSP_DEFINE_COPY_AND_MOVE (Mute);

private:
    std::bitset<m_maxChannels> m_channelsToMute;
};

HART_DSP_DECLARE_ALIASES_FOR(Mute)

}  // namespace hart
