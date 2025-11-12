#pragma once

#include <memory>
#include <string>

#include "dr_wav.h"

#include "hart_audio_buffer.hpp"
#include "signals/hart_signal.hpp"
#include "hart_utils.hpp"  // toAbsolutePath()

namespace hart
{

template<typename SampleType>
class WavFile:
    public Signal<SampleType>
{
public:
    enum class Loop
    {
        yes,
        no
    };

    WavFile (const std::string& filePath, Loop loop = Loop::no):
        m_filePath (filePath),
        m_loop (loop)
    {
        drwav_uint64 numFrames;
        unsigned int numChannels;
        unsigned int wavSampleRateHz;

        float* pcmFrames = drwav_open_file_and_read_pcm_frames_f32 (
            toAbsolutePath (filePath).c_str(),
            &numChannels,
            &wavSampleRateHz,
            &numFrames,
            nullptr
            );

        // TODO: Warning message
        if (pcmFrames == nullptr)
            return;

        m_wavFrames = std::make_shared<AudioBuffer<float>> (numChannels, numFrames);

        for (size_t frame = 0; frame < numFrames; ++frame)
            for (size_t channel = 0; channel < numChannels; ++channel)
                (*m_wavFrames)[channel][frame] = pcmFrames[frame * numChannels + channel];

        drwav_free (pcmFrames, nullptr);

        m_wavSampleRateHz = static_cast<double> (wavSampleRateHz);
        m_wavNumChannels = static_cast<int> (numChannels);
    }

    void prepare (double sampleRateHz, size_t numOutputChannels, size_t /*maxBlockSizeFrames*/) override
    {
        // TODO: Warning message is sample rate or channel num mismatches the wav file
    }

    void renderNextBlock (SampleType* const* outputs, size_t numFrames) override
    {
        // TODO: Add support for number of channels different from the wav file
        const size_t numChannels = m_wavNumChannels;
        size_t frameInOutputBuffer = 0;
        size_t frameInWavBuffer = m_wavOffsetFrames;

        while (m_wavOffsetFrames < m_wavFrames->getNumFrames() && frameInOutputBuffer < numFrames)
        {
            for (size_t channel = 0; channel < m_wavNumChannels; ++channel)
                outputs[channel][frameInOutputBuffer] = (*m_wavFrames)[channel][frameInWavBuffer];

            ++frameInOutputBuffer;
            ++frameInWavBuffer;
            ++m_wavOffsetFrames;

            if (m_loop == Loop::yes)
                m_wavOffsetFrames %= m_wavFrames->getNumFrames();
        }

        while (frameInOutputBuffer < numFrames)
        {
            // TODO: assert m_loop == Loop::no
            for (size_t channel = 0; channel < m_wavNumChannels; ++channel)
                outputs[channel][frameInOutputBuffer] = (SampleType) 0;

            ++frameInOutputBuffer;
        }
    }

    void reset() override
    {
        m_wavOffsetFrames = 0;
    }

    std::unique_ptr<Signal<SampleType>> copy() const override
    {
        return std::make_unique<WavFile<SampleType>> (*this);
    }

    std::string describe() const override
    {
        return std::string ("WavFile (\"") + m_filePath + (m_loop == Loop::yes ? "\", Loop::yes)" : "\", Loop::no)");
    }

private:
    const std::string m_filePath;
    const Loop m_loop;
    int m_wavNumChannels;
    double m_wavSampleRateHz;
    size_t m_wavOffsetFrames = 0;
    std::shared_ptr<AudioBuffer<float>> m_wavFrames;
};

}  // namespace hart
