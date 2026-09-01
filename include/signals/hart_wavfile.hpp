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
/// @details Original levels from the wav file are preserved. If sample rate requested
/// by the test runner is different from original waf file's SR, it will quietly re-sample,
/// so that any sample rate is supported.
/// @ingroup Signals
template<typename SampleType>
class WavFile:
    public Signal<SampleType, WavFile<SampleType>>
{
public:
    /// @brief Instructs WavFile whether to resample audio, if requested Sample Rate doesn't match original SR of the wav file
    enum class Resample
    {
        no,   ///< If sample of test runner doesn't match the wav file's SR, it will refuse to render audio
        yes   ///< If sample of test runner doesn't match the wav file's SR, it will quietly re-sample
    };

    /// @brief Creates a Signal that produces audio from a wav file
    /// @param filePath Path to a wav file
    /// Can be absolute or relative. If a relative path is used, it will resolve
    /// as relative to a data root path provided via respective CLI argument.
    /// @see HART_REQUIRES_DATA_PATH_ARG
    /// @param loop Indicates whether the signal should loop the audio or produce
    /// @param resample Indicates whether the audio should be re-sampled if test
    /// runner's Sample Rate doesn't match the wav's SR, see `Resample`.
    /// silence after wav file runs out of frames.
    WavFile (const std::string& filePath, Loop loop = Loop::no, Resample resample = Resample::yes):
        m_filePath (filePath),
        m_loop (loop),
        m_resample (resample)
    {
        const std::string fileAbsolutePath = toAbsolutePath (filePath);
        
        if (! fileExistsAndReadable (fileAbsolutePath))
            HART_THROW_OR_RETURN_VOID (hart::IOError, "Wav file does not exist, or not readable");

        drwav_uint64 numFrames;
        unsigned int numChannels;
        unsigned int wavSampleRateHz;

        float* pcmFrames = drwav_open_file_and_read_pcm_frames_f32 (
            fileAbsolutePath.c_str(),
            &numChannels,
            &wavSampleRateHz,
            &numFrames,
            nullptr
            );

        if (pcmFrames == nullptr)
            HART_THROW_OR_RETURN_VOID (hart::IOError, std::string ("Could not read frames from the wav file"));

        m_wavFramesOriginal = std::make_shared<AudioBuffer<SampleType>> (
            static_cast<size_t> (numChannels),
            static_cast<size_t> (numFrames),
            static_cast<double> (wavSampleRateHz)
        );

        for (size_t frame = 0; frame < numFrames; ++frame)
            for (size_t channel = 0; channel < numChannels; ++channel)
                (*m_wavFramesOriginal)[channel][frame] = static_cast<SampleType> (pcmFrames[frame * numChannels + channel]);

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
        if (m_resample == Resample::yes)
            return sampleRateHz > 0.0;

        // If Resample::no was selected instead:

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
        const bool needsResampling = floatsNotEqual (sampleRateHz, m_wavFramesOriginal->getSampleRateHz(), m_sampleRateToleranceHz);

        if (needsResampling)
        {
            hassert (m_resample == Resample::yes);  // Should have rejected mismatched SR if Resample::no was selected
            hassert (sampleRateHz > 0.0);  // Test runner should catch invalid sample rate values

            if (m_wavFramesResampled == nullptr || floatsNotEqual (m_wavFramesResampled->getSampleRateHz(), sampleRateHz, m_sampleRateToleranceHz))
                m_wavFramesResampled = std::make_shared<AudioBuffer<SampleType>> (m_wavFramesOriginal->resample (sampleRateHz));

            m_wavFramesSource = m_wavFramesResampled;
        }
        else
        {
            m_wavFramesSource = m_wavFramesOriginal;
        }

        hassert (m_wavFramesSource != nullptr);
        hassert (floatsEqual (m_wavFramesSource->getSampleRateHz(), sampleRateHz, m_sampleRateToleranceHz));
    }

    void renderNextBlock (AudioBuffer<SampleType>& output) override
    {
        // TODO: Add support for number of channels different from the wav file

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
    const Resample m_resample;
    size_t m_wavOffsetFrames = 0;
    std::shared_ptr<AudioBuffer<SampleType>> m_wavFramesOriginal;
    std::shared_ptr<AudioBuffer<SampleType>> m_wavFramesResampled;
    std::shared_ptr<AudioBuffer<SampleType>> m_wavFramesSource;
};

HART_SIGNAL_DECLARE_ALIASES_FOR (WavFile)

}  // namespace hart
