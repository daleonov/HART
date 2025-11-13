#pragma once

#include <cmath>  // delete me
#include <string>
#include <vector>

#include "dr_wav.h"

#include "hart_audio_buffer.hpp"
#include "hart_exceptions.hpp"
#include "hart_units.hpp"
#include "hart_wavformat.hpp"

namespace hart
{

template <typename SampleType>
class WavWriter
{
public:
    static void writeBuffer (const AudioBuffer<SampleType>& buffer, const std::string& fileName, double sampleRateHz, WavFormat wavFormat = WavFormat::pcm24)
    {
        const size_t numFrames = buffer.getNumFrames();
        const size_t numChannels = buffer.getNumChannels();

        drwav drWavHandle;
        drwav_data_format drWavFormat;
        drWavFormat.container = drwav_container_riff;
        drWavFormat.channels = static_cast<drwav_uint16> (numChannels);
        drWavFormat.sampleRate = static_cast<drwav_uint32> (sampleRateHz);

        switch (wavFormat)
        {
            case WavFormat::pcm16:
                drWavFormat.format = DR_WAVE_FORMAT_PCM;
                drWavFormat.bitsPerSample = 16;
                break;
            case WavFormat::pcm24:
                drWavFormat.format = DR_WAVE_FORMAT_PCM;
                drWavFormat.bitsPerSample = 24;
                break;
            case WavFormat::pcm32:
                drWavFormat.format = DR_WAVE_FORMAT_PCM;
                drWavFormat.bitsPerSample = 32;
                break;
            case WavFormat::float32:
                drWavFormat.format = DR_WAVE_FORMAT_IEEE_FLOAT;
                drWavFormat.bitsPerSample = 32;
                break;
        }

        if (! drwav_init_file_write (&drWavHandle, fileName.c_str(), &drWavFormat, nullptr))
            HART_THROW_OR_RETURN_VOID (hart::IOError, "Failed to init WAV writer");

        switch (wavFormat)
        {
            case WavFormat::float32:
            {
                std::vector<float> pcmData (numFrames * numChannels);

                for (size_t frame = 0; frame < numFrames; ++frame)
                {
                    for (size_t channel = 0; channel < numChannels; ++channel)
                        pcmData[frame * numChannels + channel] = buffer[channel][frame];
                }

                drwav_write_pcm_frames (&drWavHandle, numFrames, pcmData.data());
                break;
            }

            case WavFormat::pcm16:
            {
                std::vector<int16_t> pcmData (numFrames * numChannels);
                constexpr double scale = 32767.0;

                for (size_t frame = 0; frame < numFrames; ++frame)
                {
                    for (size_t channel = 0; channel < numChannels; ++channel)
                    {
                        const double sample = static_cast<double> (buffer[channel][frame]);
                        const int32_t pcmValue = static_cast<int32_t> (std::round (hart::clamp (scale * sample, -scale, scale)));
                        pcmData[frame * numChannels + channel] = pcmValue;
                    }
                }

                drwav_write_pcm_frames (&drWavHandle, numFrames, pcmData.data());
                break;
            }

            case WavFormat::pcm32:
            {
                std::vector<int32_t> pcmData (numFrames * numChannels);
                constexpr double scale = 2147483647.0;

                for (size_t frame = 0; frame < numFrames; ++frame)
                {
                    for (size_t channel = 0; channel < numChannels; ++channel)
                    {
                        const double sample = static_cast<double> (buffer[channel][frame]);
                        const int32_t pcmValue = static_cast<int32_t> (std::round (hart::clamp (scale * sample, -scale, scale)));
                        pcmData[frame * numChannels + channel] = pcmValue;
                    }
                }

                drwav_write_pcm_frames (&drWavHandle, numFrames, pcmData.data());
                break;
            }

            case WavFormat::pcm24:
            {
                std::vector<uint8_t> pcmData (numFrames * numChannels * 3);
                constexpr double scale = 8388607.0;

                for (size_t frame = 0; frame < numFrames; ++frame)
                {
                    for (size_t channel = 0; channel < numChannels; ++channel)
                    {
                        const double sample = static_cast<double> (buffer[channel][frame]);
                        const int32_t pcmValue = static_cast<int32_t> (std::round (hart::clamp (scale * sample, -scale, scale)));
                        size_t idx = (frame * numChannels + channel) * 3;
                        pcmData[idx] = static_cast<uint8_t> (pcmValue & 0xff);
                        pcmData[idx + 1] = static_cast<uint8_t> ((pcmValue >> 8) & 0xff);
                        pcmData[idx + 2] = static_cast<uint8_t> ((pcmValue >> 16) & 0xff);
                    }
                }

                drwav_write_pcm_frames (&drWavHandle, numFrames, pcmData.data());
                break;
            }
        }

        drwav_uninit (&drWavHandle);
    }
};

} // namespace hart
