#include <ostream>

#include "hart.hpp"
#include "hart_audio_buffer.hpp"
#include "dsp/hart_dsp_all.hpp"
#include "example_processors.hpp"

class TestedBoosterProcessor:
    public hart::DSP<float>
{
public:
    enum Params
    {
        gainDb
    };

    TestedBoosterProcessor (hart::examples::LinearStereoBooster& booster):
        m_booster (booster)
    {
    }

    void prepare (double sampleRateHz, size_t numInputChannels, size_t numOutputChannels, size_t maxBlockSizeFrames) override
    {
        if (numInputChannels != 2 || numOutputChannels != 2)
            throw std::runtime_error ("Booster only supports stereo (2 in, 2 out channels)");
    }

    void process (const hart::AudioBuffer<float>& inputs, hart::AudioBuffer<float>& outputs) override
    {
        m_booster.process (inputs.getArrayOfReadPointers(), outputs.getArrayOfWritePointers(), inputs.getNumFrames());
    }

    void reset() override {}

    void setValue (int id, float value) override
    {
        if (id == Params::gainDb)
            m_booster.setGainDb (value);
    }

    double getValue (int id) const override
    {
        if (id == Params::gainDb)
            return m_booster.getGainDb();

        return 0.0;
    }

    bool supportsChannelLayout (size_t numInputChannels, size_t numOutputChannels) const override
    {
        return numInputChannels == 2 && numOutputChannels == 2;
    }

    void print (std::ostream& stream) const override
    {
        stream << "TestedBoosterProcessor()";
    }

    bool supportsEnvelopeFor (int id) const override
    {
        return false;
    }

    HART_DSP_DECLARE_COPY_METHOD (TestedBoosterProcessor);

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
        .withValue (TestedBoosterProcessor::gainDb, 10.0f)
        .inStereo()
        .expectTrue (hart::equalsTo (hart::Silence<float>()))
        .process();
}

HART_TEST ("Booster: Gain")
{
    hart::examples::LinearStereoBooster booster;
    TestedBoosterProcessor testedBoosterProcessor (booster);

    hart::processAudioWith (testedBoosterProcessor)
        .withInputSignal (hart::SineWave<float>())
        .withSampleRate (44100.0)
        .withBlockSize (1024)
        .withDuration (0.01)
        .withValue (TestedBoosterProcessor::gainDb, 0.0f)
        .inStereo()
        .expectTrue (hart::equalsTo (hart::SineWave<float>()))
        .process();

    hart::processAudioWith(testedBoosterProcessor)
        .withInputSignal (hart::WhiteNoise<float>())
        .withSampleRate (44100.0)
        .withBlockSize (1024)
        .withDuration (0.1)
        .withValue (TestedBoosterProcessor::gainDb, 0.0f)
        .inStereo()
        .expectFalse (hart::equalsTo (hart::SineWave<float>()))
        .saveOutputTo ("Booster Gain Noise Out.wav", hart::Save::always, hart::WavFormat::pcm24)
        .process();

    hart::processAudioWith (testedBoosterProcessor)
        .withInputSignal (hart::SineWave<float>())
        .withSampleRate (44100.0_Hz)
        .withBlockSize (1024)
        .withDuration (0.1)
        .withValue (TestedBoosterProcessor::gainDb, -3.0f)
        .inStereo()
        .assertFalse (hart::equalsTo (hart::SineWave<float>()))
        .expectTrue (hart::peaksBelow (-2.9f))
        .saveOutputTo ("Booster Gain Out.wav", hart::Save::always, hart::WavFormat::pcm24)
        .process();
}
