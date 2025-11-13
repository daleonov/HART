#pragma once

#include <cstdint>
#include <memory>
#include <random>
#include <string>

#include "hart_cliconfig.hpp"
#include "signals/hart_signal.hpp"

namespace hart
{

template<typename SampleType>
class WhiteNoise : public Signal<SampleType>
{
public:
    WhiteNoise (uint_fast32_t randomSeed = CLIConfig::get().getRandomSeed()):
        m_randomSeed (randomSeed)
    {
        reset();
    }

    void prepare (double /*sampleRateHz*/, size_t numOutputChannels, size_t /*maxBlockSizeFrames*/) override
    {
        this->setNumChannels (numOutputChannels);
    }

    void renderNextBlock (AudioBuffer<SampleType>& output) override
    {
        for (size_t frame = 0; frame < output.getNumFrames(); ++frame)
            for (size_t channel = 0; channel < this->m_numChannels; ++channel)
                output[channel][frame] = m_uniformRealDistribution (m_randomNumberGenerator);
    }

    void reset() override
    {
        m_randomNumberGenerator = std::mt19937 (m_randomSeed);
        m_uniformRealDistribution.reset();
    }

    std::unique_ptr<Signal<SampleType>> copy() const override
    {
        return std::make_unique <WhiteNoise<SampleType>> (*this);
    }

    std::string describe() const override
    {
        return std::string ("WhiteNoise (" + std::to_string (m_randomSeed) + ")");
    }

private:
    const uint_fast32_t m_randomSeed;
    std::mt19937 m_randomNumberGenerator;
    std::uniform_real_distribution<SampleType> m_uniformRealDistribution {(SampleType) -1, (SampleType) 1};
};

}  // namespace hart
