#pragma once

#include <cstdint>  // uint_fast32_t
#include <random>

#include "hart_cliconfig.hpp"
#include "hart_dsp.hpp"
#include "hart_exceptions.hpp"
#include "hart_precision.hpp"
#include "hart_utils.hpp"  // decibelsToRatio()

namespace hart
{

/// @brief Mixes additive white noise to the signal
/// @details Can operate either in symmetricall channel configurations, i.e.
/// same number of inputs and outputs, or mono input to many outputs. Either
/// way, each output channel with get its own noise realization.
/// As anything in HART, the noise is deterministic, and can be controlled
/// using a custom seed, either by specifying one as CLI argument, or by
/// supplying one to the ctor directly.
/// Repeated renders with the same seed produce bit-identical noise.
/// @ingroup DSP
template <typename SampleType>
class AdditiveNoise:
    public hart::DSP<SampleType, AdditiveNoise<SampleType>>
{
public:
    enum Params
    {
        noiseLevelDb  ///< Noise sample peak level in dB
    };

    /// @brief Creates a DSP that mixes white noise to the signal
    /// @param initialNoiseLevelDb Noise sample peak level in dB (prior to mixing to the signal).
    /// Can be overridden at a later point by calling `setValue()` with a `Param::noiseLevelDb` argument.
    /// @param randomSeed Seed for noise generation.
    AdditiveNoise (double initialNoiseLevelDb = 0.0, uint_fast32_t randomSeed = CLIConfig::getInstance().getRandomSeed()) :
        m_initialNoiseLevelDb (initialNoiseLevelDb),
        m_noiseLevelLinear (static_cast<SampleType> (decibelsToRatio (initialNoiseLevelDb))),
        m_randomSeed (randomSeed),
        m_randomNumberGenerator (randomSeed)
        {
        }

    void prepare (double /* sampleRateHz */, size_t /* numInputChannels */, size_t /* numOutputChannels */, size_t /* maxBlockSizeFrames */) override {}

    void process (const AudioBuffer<SampleType>& input, AudioBuffer<SampleType>& output, const EnvelopeBuffers& /* envelopeBuffers */, ChannelFlags channelsToProcess) override
    {
        const bool symmetricalChannelLayout = input.getNumChannels() == output.getNumChannels();
        const bool monoToMultiChannelLayout = input.getNumChannels() == 1 && output.getNumChannels() > 1;

        if (! symmetricalChannelLayout && ! monoToMultiChannelLayout)
        {
            hassertfalse;  // Unexpected channel layout
            return;
        }

        for (size_t outputChannel = 0; outputChannel < output.getNumChannels(); ++outputChannel)
        {
            const size_t inputChannel = symmetricalChannelLayout ? outputChannel : 0;
            const SampleType* inputChannelData = input[inputChannel];
            SampleType* outputChannelData = output[outputChannel];

            if (channelsToProcess[outputChannel] == true)
            {
                for (size_t frame = 0; frame < input.getNumFrames(); ++frame)
                    outputChannelData[frame] = inputChannelData[frame] + m_noiseLevelLinear * m_uniformRealDistribution (m_randomNumberGenerator);
            }
            else
            {
                for (size_t frame = 0; frame < input.getNumFrames(); ++frame)
                    outputChannelData[frame] = inputChannelData[frame];
            }
        }
    }

    /// @brief Sets additive noise level
    /// @param id Only `Param::noiseLevelDb` is accepted
    /// @param value Noise sample peak level in dB (prior to mixing to the signal).
    void setValue (int paramId, double value) override
    {
        if (paramId != Params::noiseLevelDb)
            HART_THROW_OR_RETURN_VOID (hart::ValueError, "Unsupported value ID");

        m_noiseLevelLinear = static_cast<SampleType> (decibelsToRatio (value));
    }

    void reset() override
    {
        m_randomNumberGenerator = std::mt19937 (m_randomSeed);
        m_uniformRealDistribution.reset();
    }

    /// @brief Checks support for a specific channel layout
    /// @details Can operate either in symmetrical channel configurations, i.e.
    /// same number of inputs and outputs, or mono input to many outputs.
    bool supportsChannelLayout (size_t numInputChannels, size_t numOutputChannels) const override
    {
        return numInputChannels == numOutputChannels
            || (numInputChannels == 1 && numOutputChannels > 1);
    }

    virtual void represent (std::ostream& stream) const override
    {
        // Updated level values set by setValue() won't be displayed here,
        // just whatever it was constructed with.

        stream
            << "AdditiveNoise ("
            << dbPrecision << m_initialNoiseLevelDb << "_dB, "
            << m_randomSeed << ')';
    }

    /// @brief No envelope support yet.
    /// @note Envelope support can be implemented easily enough, but only
    /// when there's a good reason to have it. Keeping things simple for now.
    bool supportsEnvelopeFor (int /* id */) const override
    {
        return false;
    }

    HART_DSP_COPYABLE (AdditiveNoise);

private:
    double m_initialNoiseLevelDb;
    SampleType m_noiseLevelLinear;
    uint_fast32_t m_randomSeed;

    std::mt19937 m_randomNumberGenerator;
    std::uniform_real_distribution<SampleType> m_uniformRealDistribution {(SampleType) -1, (SampleType) 1};
};

HART_DSP_DECLARE_ALIASES_FOR (AdditiveNoise)

}  // namespace hart
