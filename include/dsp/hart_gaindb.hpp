#pragma once

#include <algorithm>  // fill()
#include <iomanip>
#include <vector>

#include "hart_dsp.hpp"
#include "hart_precision.hpp"
#include "hart_utils.hpp"

namespace hart
{

/// @brief Applies gain in decibels to the signal
/// @note For automation, consider using @ref GainLinear instead, because the two classes produce different curve shapes.
/// @ingroup DSP
template <typename SampleType>
class GainDb:
    public hart::DSP<SampleType, GainDb<SampleType>>
{
public:
    enum Params
    {
        gainDb  ///< Gain in decibels
    };

    /// @brief Constructor
    /// @param gainDb Initial gain in decibels
    GainDb (double gainDb = 0.0):
        m_initialGainDb (gainDb),
        m_gainLinear (decibelsToRatio (gainDb))
    {
    }

    void prepare (double /* sampleRateHz */, size_t /* numInputChannels */, size_t /* numOutputChannels */, size_t maxBlockSizeFrames) override
    {
        m_gainEnvelopeValuesLinear.resize (this->hasEnvelopeFor (Params::gainDb) ? maxBlockSizeFrames : 0);
    }

    void process (const AudioBuffer<SampleType>& input, AudioBuffer<SampleType>& output, const EnvelopeBuffers& envelopeBuffers, ChannelFlags channelsToProcess) override
    {
        const size_t numInputChannels = input.getNumChannels();
        const size_t numOutputChannels = output.getNumChannels();
        hassert (output.getNumFrames() == input.getNumFrames());

        if (! supportsChannelLayout (numInputChannels, numOutputChannels))
            HART_THROW_OR_RETURN_VOID (hart::ChannelLayoutError, "Unsupported channel configuration");

        const bool hasGainEnvelope = ! envelopeBuffers.empty() && contains (envelopeBuffers, (int) Params::gainDb);

        if (hasGainEnvelope)
        {
            auto& gainEnvelopeValuesDb = envelopeBuffers. at(Params::gainDb);
            hassert (gainEnvelopeValuesDb.size() == m_gainEnvelopeValuesLinear.size());
            
            for (size_t i = 0; i < m_gainEnvelopeValuesLinear.size(); ++i)
                m_gainEnvelopeValuesLinear[i] = decibelsToRatio (gainEnvelopeValuesDb[i]);

            processEnvelopedGain (input, output, channelsToProcess);
            return;
        }

        // No gain envelope
        processConstantGain (input, output, channelsToProcess);
    }

    void reset() override {}

    /// @param id Only @ref GainDb::gainDb is accepted
    /// @param value Gain in decibels
    void setValue (int id, double value) override
    {
        if (id == Params::gainDb)
            m_gainLinear = decibelsToRatio (value);
    }

    /// @param id Only @ref GainDb::gainDb is accepted
    /// @retval (value) Gain in decibels
    double getValue (int id) const override
    {
        if (id == Params::gainDb)
            return ratioToDecibels (m_gainLinear);

        return 0.0;
    }

    /// @brief Checks if the DSP supports given i/o configuration
    /// @details Supports only n-to-n configurations 
    virtual bool supportsChannelLayout (size_t numInputChannels, size_t numOutputChannels) const override
    {
        return numInputChannels == numOutputChannels;
    }

    virtual void represent (std::ostream& stream) const override
    {
        stream << dbPrecision << "GainDb (" << m_initialGainDb << "_dB)";
    }

    /// @param id Only @ref GainDb::gainDb is accepted
    /// retval (bool) true for GainDb::gainDb, else otherwise
    bool supportsEnvelopeFor (int id) const override
    {
        return id == Params::gainDb;
    }

    HART_DSP_COPYABLE (GainDb);

private:
    double m_initialGainDb;
    double m_gainLinear;
    std::vector<double> m_gainEnvelopeValuesLinear;

    void processConstantGain (const AudioBuffer<SampleType>& input, AudioBuffer<SampleType>& output, ChannelFlags channelsToProcess)
    {
        for (size_t channel = 0; channel < input.getNumChannels(); ++channel)
        {
            if (channelsToProcess[channel] == true)
            {
                for (size_t frame = 0; frame < input.getNumFrames(); ++frame)
                    output[channel][frame] = input[channel][frame] * (SampleType) m_gainLinear;
            }
            else
            {
                for (size_t frame = 0; frame < input.getNumFrames(); ++frame)
                    output[channel][frame] = input[channel][frame];
            }
        }
    }

    void processEnvelopedGain (const AudioBuffer<SampleType>& input, AudioBuffer<SampleType>& output, ChannelFlags channelsToProcess)
    {
        for (size_t channel = 0; channel < input.getNumChannels(); ++channel)
        {
            if (channelsToProcess[channel] == true)
            {
                for (size_t frame = 0; frame < input.getNumFrames(); ++frame)
                    output[channel][frame] = input[channel][frame] * (SampleType) m_gainEnvelopeValuesLinear[frame];
            }
            else
            {
                for (size_t frame = 0; frame < input.getNumFrames(); ++frame)
                    output[channel][frame] = input[channel][frame];
            }

        }
    }
};

HART_DSP_DECLARE_ALIASES_FOR (GainDb)

}  // namespace hart
