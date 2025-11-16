#pragma once

#include "hart_dsp.hpp"

#include <algorithm>  // min(), max()

namespace hart
{

/// @brief Applies symmetrical hard clipping (no knee) to the signal.
/// @details Signal will never go above the threshold. Signal that doesn't peak above
///         the threshold is guaranteed to be identical to the input signal.
/// @ingroup DSP
template <typename SampleType>
class HardClip:
    public hart::DSP<SampleType>
{
public:
    enum Params
    {
        thresholdDb  ///< Threshold in decibels
    };

    /// @brief Contructor
    /// @param thresholdDb Fixed threshold in decibels, above which the signal will be symmetrically hard-clipped
    HardClip (double thresholdDb = 0.0):
        m_initialThresholdDb (thresholdDb),
        m_thresholdLinear (decibelsToRatio (thresholdDb))
        {
        }

    void prepare (double /*sampleRateHz*/, size_t /*numInputChannels*/, size_t /*numOutputChannels*/, size_t /*maxBlockSizeFrames*/) override {}

    void process (const AudioBuffer<SampleType>& input, AudioBuffer<SampleType>& output, const EnvelopeBuffers& /* envelopeBuffers */) override
    {
        hassert (output.getNumFrames() == input.getNumFrames());
        const size_t numChannels = input.getNumChannels();
        const size_t numFrames = input.getNumFrames();

        if (input.getNumChannels() != output.getNumChannels())
            HART_THROW_OR_RETURN_VOID (hart::ChannelLayoutError, "Unsupported channel configuration");

        for (size_t channel = 0; channel < numChannels; ++channel)
            for (size_t frame = 0; frame < numFrames; ++frame)
                output[channel][frame] = std::min (std::max (input[channel][frame], (SampleType) -m_thresholdLinear), (SampleType) m_thresholdLinear);
    }

    void reset() override {}

    /// @param id Only @ref HardClip::thresholdDb is accepted
    /// @param value Threshold in decibels
    void setValue(int id, double value) override
    {
        if (id == Params::thresholdDb)
            m_thresholdLinear = decibelsToRatio (value);
    }

    /// @param id Only @ref HardClip::thresholdDb is accepted
    /// @return Threshold in decibels
    double getValue (int id) const override
    {
        if (id == Params::thresholdDb)
            return ratioToDecibels (m_thresholdLinear);

        return 0.0;
    }

    /// @details Supports only n-to-n channel configurations 
    bool supportsChannelLayout (size_t numInputChannels, size_t numOutputChannels) const override
    {
        return numInputChannels == numOutputChannels;
    }

    void represent (std::ostream& stream) const override
    {
        stream << "HardClip (" << m_initialThresholdDb << ")";
    }

    /// @param id Only @ref HardClip::thresholdDb is accepted
    /// @return true for @ref HardClip::thresholdDb, false otherwise
    bool supportsEnvelopeFor (int id) const override
    {
        return false;
    }

    HART_DSP_DEFINE_COPY_AND_MOVE (HardClip);

private:
    double m_initialThresholdDb;
    double m_thresholdLinear;
};

}  // namespace hart
