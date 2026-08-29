#pragma once

#include <cstdint>
#include <memory>
#include <random>
#include <string>

#include "hart_cliconfig.hpp"
#include "signals/hart_signal.hpp"

namespace hart
{

/// @brief Produces deterministic white noise
/// @details Outputs a signal uniformly distrubuted between -1.0 and +1.0, thus peaking below 0 dB.
///
/// This signal is an infinite source, constructed in time domain. You may also use
/// `Spectrum::colouredNoise()`, combined with `Spectrum::toSignal()`, to get a
/// finite signal source with ideal frequency response, if you want the highest
/// frequency responce accuracy, and don't mind its finite nature, and substantially
/// higher CPU cost.
/// @ingroup Signals
template<typename SampleType>
class WhiteNoise:
    public Signal<SampleType, WhiteNoise<SampleType>>
{
public:

    /// @brief Creates a Signal that produces white noise
    /// @param randomSeed Seed for the RNG
    /// @details Two signals with the same seed are guaranteed to produce the identical audio
    WhiteNoise (uint_fast32_t randomSeed = CLIConfig::getInstance().getRandomSeed()):
        m_randomSeed (randomSeed)
    {
        reset();
    }

    bool supportsNumChannels (size_t /* numChannels */) const override { return true; };

    void prepare (double /*sampleRateHz*/, size_t numOutputChannels, size_t /*maxBlockSizeFrames*/) override
    {
        this->setNumChannels (numOutputChannels);
    }

    void renderNextBlock (AudioBuffer<SampleType>& output) override
    {
        for (size_t frame = 0; frame < output.getNumFrames(); ++frame)
            for (size_t channel = 0; channel < this->getNumChannels(); ++channel)
                output[channel][frame] = m_uniformRealDistribution (m_randomNumberGenerator);
    }

    /// @copybrief Signal::reset()
    /// @details After resetting, WhiteNoise is guaranteed to produce identical audio to the one produced after instantiation
    void reset() override
    {
        m_randomNumberGenerator = std::mt19937 (m_randomSeed);
        m_uniformRealDistribution.reset();
    }

    void represent (std::ostream& stream) const override
    {
        stream << "WhiteNoise (" << m_randomSeed << ")";
    }

private:
    const uint_fast32_t m_randomSeed;
    std::mt19937 m_randomNumberGenerator;
    std::uniform_real_distribution<SampleType> m_uniformRealDistribution {(SampleType) -1, (SampleType) 1};
};

HART_SIGNAL_DECLARE_ALIASES_FOR (WhiteNoise)

}  // namespace hart
