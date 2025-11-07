#include <iostream>
#include "hart.hpp"
#include "example_processors.hpp"

class TestedBoosterProcessor:
    public hart::TestedAudioProcessor<float, float>
{
public:
    TestedBoosterProcessor (hart::examples::LinearStereoBooster& booster):
        m_booster (booster)
    {
    }

    void prepare (double sampleRateHz, size_t numChannels, size_t maxBlockSizeFrames) override
    {
        if (numChannels != 2)
            throw std::runtime_error ("Booster only supports stereo (2 channels)");
    }

    void process (const float** inputs, float** outputs, size_t numFrames) override
    {
        m_booster.process (inputs, outputs, numFrames);
    }

    void setValue (const std::string& id, float value) override
    {
        if (id == "Gain dB")
            m_booster.setGainDb (value);
    }

    float getValue(const std::string& id) override
    {
        if (id == "Gain dB")
            return m_booster.getGainDb();

        return 0.0f;
    }

private:
    hart::examples::LinearStereoBooster& m_booster;
};

HART_TEST ("Booster: Silence in - Silence out")
{
    hart::examples::LinearStereoBooster booster;
    TestedBoosterProcessor testedBoosterProcessor (booster);
}
