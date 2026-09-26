#pragma once

#include <cstdint>
#include <memory>
#include <random>
#include <sstream>

#include "hart_cliconfig.hpp"
#include "signals/hart_signal.hpp"

namespace hart
{

/// @brief Produces binary noise, a.k.a. Rademacher, or Bernoulli noise
/// @details Each sample will have one of two possible values: either `-1`, or
/// `+1`. Like other noise signals in HART, it's guaranteed to be deterministic,
/// i.e. identical seeds will produce identical sequences of sample values.
/// @ingroup Signals
template<typename SampleType>
class BinaryNoise:
    public Signal<SampleType, BinaryNoise<SampleType>>
{
public:
    /// @brief Creates a Signal that produces binary noise
    /// @param randomSeed Seed for the RNG, defaults to global seed set by the CLI config
    /// @details Two signals with the same seed are guaranteed to produce the identical audio
    BinaryNoise (uint_fast32_t randomSeed = CLIConfig::getInstance().getRandomSeed()):
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
                output[channel][frame] = static_cast<SampleType> (m_randomNumberGenerator() & 1u) * SampleType (2) - SampleType (1);
    }

    /// @copybrief Signal::reset()
    /// @details After resetting, this Signal is guaranteed to produce identical audio to the one produced after instantiation
    void reset() override
    {
        m_randomNumberGenerator = std::mt19937 (m_randomSeed);
    }

    void represent (std::ostream& stream) const override
    {
        stream << "BinaryNoise (" << m_randomSeed << ")";
    }

private:
    const uint_fast32_t m_randomSeed;
    std::mt19937 m_randomNumberGenerator;
};

HART_SIGNAL_DECLARE_ALIASES_FOR (BinaryNoise)

}  // namespace hart
