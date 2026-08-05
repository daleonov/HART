#pragma once

#include <memory>

#include "hart_audio_buffer.hpp"
#include "hart_impulse_response.hpp"
#include "hart_spectrum.hpp"

namespace hart
{

/// @brief Contains audio-related artefacts useful for analysis by matchers
/// @details Designed to be used to retreive various audio-related artefacts
/// for audio analysis, such as input and output audio buffers, spectra, and
/// possibly some others.
///
/// All derivative artefacts, such as Spectra, are calculated on demand and
/// then cached. Everything is read only.
///
/// Designed as a lightweight object, so it's totally okay to copy it.
/// @tparam Type of samples of the audio buffers, typicalle `float` or `double`
template <typename SampleType>
class AnalysisContext
{
public:
    /// @brief Creates an instance of analysis context
    /// @details You probably have no reason to instantiate this class yourself,
    /// it's meand to be created by the AudioTestBuilder only.
    AnalysisContext (
        const AudioBuffer<SampleType>& inputAudio,
        const AudioBuffer<SampleType>& outputAudio
    ):
        m_inputAudio (inputAudio),
        m_outputAudio (outputAudio),
        m_cache (std::make_shared<Cache>())
    {
    }

    /// @brief Returns a buffer with input audio
    const AudioBuffer<SampleType>& inputAudio() const
    {
        return m_inputAudio;
    }

    /// @brief Returns a buffer with output audio
    const AudioBuffer<SampleType>& outputAudio() const
    {
        return m_outputAudio;
    }

    /// @brief Returns a spectrum of the input audio
    const Spectrum& inputSpectrum() const
    {
        if (m_cache->inputSpectrum == nullptr)
            m_cache->inputSpectrum = std::make_shared<Spectrum> (m_inputAudio);

        return *m_cache->inputSpectrum;
    }

    /// @brief Returns a spectrum of the output audio
    const Spectrum& outputSpectrum() const
    {
        if (m_cache->outputSpectrum == nullptr)
            m_cache->outputSpectrum = std::make_shared<Spectrum> (m_outputAudio);

        return *m_cache->outputSpectrum;
    }

    /// @brief Returns an impulse response of the output audio vs input audio
    const ImpulseResponse<SampleType>& impulseResponse() const
    {
        if (m_cache->impulseResponse == nullptr)
        {
            m_cache->impulseResponse = std::make_shared<ImpulseResponse<SampleType>> (
                m_inputAudio,
                m_outputAudio
                );
        }

        return *m_cache->impulseResponse;
    }

private:
    struct Cache
    {
        mutable std::shared_ptr<Spectrum> inputSpectrum = nullptr;
        mutable std::shared_ptr<Spectrum> outputSpectrum = nullptr;
        mutable std::shared_ptr<ImpulseResponse<SampleType>> impulseResponse = nullptr;
    };

    const AudioBuffer<SampleType>& m_inputAudio;
    const AudioBuffer<SampleType>& m_outputAudio;

    std::shared_ptr<Cache> m_cache;
};

}  // namespace hart
