#include <iostream>
#include "hart.hpp"
#include "example_processors.hpp"
#include "hart_process_audio.hpp"

class TestedBoosterProcessor:
    public hart::TestedAudioProcessor<float, float>
{
public:
    TestedBoosterProcessor (hart::examples::LinearStereoBooster& booster):
        m_booster (booster)
    {
    }

    void prepare (double sampleRateHz, size_t numInputChannels, size_t numOutputChannels, size_t maxBlockSizeFrames) override
    {
        if (numInputChannels != 2 || numOutputChannels != 2)
            throw std::runtime_error ("Booster only supports stereo (2 in, 2 out channels)");
    }

    void process (const float* const* inputs, float* const* outputs, size_t numFrames) override
    {
        m_booster.process (inputs, outputs, numFrames);
    }

    void reset() override {}

    void setValue (const std::string& id, float value) override
    {
        if (id == "Gain dB")
            m_booster.setGainDb (value);
    }

    float getValue (const std::string& id) override
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

    hart::processAudioWith (testedBoosterProcessor)
        .withInputSignal (hart::Silence<float>())
        .withSampleRate (44100.0)
        .withBlockSize (1024)
        .withDuration (0.1)
        .withValue ("Gain dB", 10.0f)
        .inStereo()
        .expectTrue (hart::equalsTo (hart::Silence<float>()))
        .process();
}
