#pragma once

#include <memory>
#include <string>

#include "dependencies/choc/platform/choc_DisableAllWarnings.h"
#include "dependencies/dr_libs/dr_wav.h"
#include "dependencies/choc/platform/choc_ReenableAllWarnings.h"

#include "hart_exceptions.hpp"
#include "signals/hart_signal.hpp"
#include "hart_utils.hpp"  // toAbsolutePath(), floatsNotEqual(), fileExistsAndReadable(), Loop

namespace hart
{

// TODO: Add "normalize" option?
// TODO: Add an entity that reuses wav data if a WavFile for a previously opened file gets instantiated

/// @brief Produces audio from a wav file
/// @details Original levels from the wav file are preserved
/// @ingroup Signals
template<typename SampleType>
class WavFile:
    public Signal<SampleType, WavFile<SampleType>>
{
public:
    /// @brief Creates a Signal that produces audio from a wav file
    /// @param filePath Path to a wav file
    /// Can be absolute or relative. If a relative path is used, it will resolve
    /// as relative to a data root path provided via respective CLI argument.
    /// @see HART_REQUIRES_DATA_PATH_ARG
    /// @param loop Indicates whether the signal should loop the audio or produce
    /// silence after wav file runs out of frames.
    WavFile (const std::string& filePath, Loop loop = Loop::no):
        m_filePath (filePath),
        m_loop (loop)
    {
        const std::string fileAbsolutePath = toAbsolutePath (filePath);
        
        if (! fileExistsAndReadable (fileAbsolutePath))
            HART_THROW_OR_RETURN_VOID (hart::IOError, "Wav file does not exist, or not readable");

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

        if (pcmFrames == nullptr)
            HART_THROW_OR_RETURN_VOID (hart::IOError, std::string ("Could not read frames from the wav file"));

        m_wavFramesOriginal = std::make_shared<AudioBuffer<float>> (
            static_cast<size_t> (numChannels),
            static_cast<size_t> (numFrames),
            static_cast<double> (wavSampleRateHz)
        );

        for (size_t frame = 0; frame < numFrames; ++frame)
            for (size_t channel = 0; channel < numChannels; ++channel)
                (*m_wavFramesOriginal)[channel][frame] = pcmFrames[frame * numChannels + channel];

        drwav_free (pcmFrames, nullptr);
    }

    /// @copydoc Signal::supportsNumChannels()
    /// @note WavFile can only fill as much channels as there are in the wav file, or less.
    /// For instance, if the wav file is stereo, it can generate two channels (as they are),
    /// one channel (left, discarding right), but not three channels.
    bool supportsNumChannels (size_t numChannels) const override
    {
        if (m_wavFramesOriginal == nullptr)
        {
            hassertfalse;
            return false;
        }

        return numChannels <= m_wavFramesOriginal->getNumChannels();
    };

    bool supportsSampleRate (double sampleRateHz) const override
    {
        if (m_wavFramesOriginal == nullptr)
        {
            hassertfalse;
            return false;
        }

        return floatsEqual (sampleRateHz, m_wavFramesOriginal->getSampleRateHz(), m_sampleRateToleranceHz);
    }

    void prepare (double sampleRateHz, size_t numOutputChannels, size_t /*maxBlockSizeFrames*/) override
    {
        if (m_wavFramesOriginal == nullptr)
        {
            hassertfalse;
            return;
        }

        // There are a few obvious cases where channel number mismatch can be gracefully resolved - perhaps in the future
        if (numOutputChannels != m_wavFramesOriginal->getNumChannels())
            HART_THROW_OR_RETURN_VOID (hart::ChannelLayoutError, std::string ("Unexpected channel number"));

        hassert (m_wavFramesOriginal->hasSampleRate());  // Sample rate should be assigned to the buffer in the ctor

        // TODO: Resampling is supported now, so quietly resample
        if (floatsNotEqual (sampleRateHz, m_wavFramesOriginal->getSampleRateHz(), m_sampleRateToleranceHz))
            HART_THROW_OR_RETURN_VOID (hart::UnsupportedError, std::string ("Wav file is in different sampling rate, resampling not supported"));
    
        m_wavFramesSource = m_wavFramesOriginal;
    }

    void renderNextBlock (AudioBuffer<SampleType>& output) override
    {
        // TODO: Add support for number of channels different from the wav file
        // TODO: Add resampling

        if (m_wavFramesSource == nullptr)
        {
            // Source should have been assigned during the prepare() call
            hassertfalse;
            return;
        }

        hassert (output.getNumChannels() == m_wavFramesSource->getNumChannels());
        hassert (output.hasSampleRate());
        hassert (m_wavFramesSource->hasSampleRate());
        hassert (floatsEqual (output.getSampleRateHz(), m_wavFramesSource->getSampleRateHz(), m_sampleRateToleranceHz));

        const size_t numFrames = output.getNumFrames();
        const size_t numChannels = output.getNumChannels();
        size_t frameInOutputBuffer = 0;
        size_t frameInWavBuffer = m_wavOffsetFrames;

        while (m_wavOffsetFrames < m_wavFramesSource->getNumFrames() && frameInOutputBuffer < numFrames)
        {
            for (size_t channel = 0; channel < m_wavFramesSource->getNumChannels(); ++channel)
                output[channel][frameInOutputBuffer] = (*m_wavFramesSource)[channel][frameInWavBuffer];

            ++frameInOutputBuffer;
            ++frameInWavBuffer;
            ++m_wavOffsetFrames;

            if (m_loop == Loop::yes)
                m_wavOffsetFrames %= m_wavFramesSource->getNumFrames();
        }

        while (frameInOutputBuffer < numFrames)
        {
            hassert (m_loop == Loop::no);

            for (size_t channel = 0; channel < m_wavFramesSource->getNumChannels(); ++channel)
                output[channel][frameInOutputBuffer] = (SampleType) 0;

            ++frameInOutputBuffer;
        }
    }

    void reset() override
    {
        m_wavOffsetFrames = 0;
    }

    void represent (std::ostream& stream) const override
    {
        stream << "WavFile (\"" << m_filePath << (m_loop == Loop::yes ? "\", Loop::yes)" : "\", Loop::no)");
    }

private:
    static constexpr double m_sampleRateToleranceHz = 1e-3;
    const std::string m_filePath;
    const Loop m_loop;
    size_t m_wavOffsetFrames = 0;
    std::shared_ptr<AudioBuffer<float>> m_wavFramesOriginal;
    std::shared_ptr<AudioBuffer<float>> m_wavFramesSource;
};

HART_SIGNAL_DECLARE_ALIASES_FOR (WavFile)

}  // namespace hart
