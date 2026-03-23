#pragma once

#include <functional>

#include "hart_dsp.hpp"
#include "hart_exceptions.hpp"

// TODO: Document this class

namespace hart
{

/// @ingroup DSP
template<typename SampleType>
class DSPFunction :
    public DSP<SampleType, DSPFunction<SampleType>>
{
public:
    DSPFunction (std::function<SampleType (SampleType)> dspFunction, const std::string& label = {}):
        m_dspFunctionSampleWise (std::move (dspFunction)), m_label (label)
    {
    }

    DSPFunction (std::function<void (AudioBuffer<SampleType>&)> dspFunction, const std::string& label = {}):
        m_dspFunctionBlockWiseReplacing (std::move (dspFunction)), m_label (label)
    {
    }

    DSPFunction (std::function<void (const AudioBuffer<SampleType>&, AudioBuffer<SampleType>&)> dspFunction, const std::string& label = {}):
        m_dspFunctionBlockWiseNonReplacing (std::move (dspFunction)), m_label (label)
    {
    }

    void prepare (double sampleRateHz, size_t numInputChannels, size_t numOutputChannels, size_t maxBlockSizeFrames) override
    {
        if (m_dspFunctionBlockWiseReplacing)
        {
            const size_t maxChannels = std::max (numInputChannels, numOutputChannels);
            m_inputOutputBuffer = AudioBuffer<SampleType> (maxChannels, maxBlockSizeFrames, sampleRateHz);
        }
    }

    void process (const AudioBuffer<SampleType>& input, AudioBuffer<SampleType>& output, const EnvelopeBuffers& /* envelopeBuffers */, ChannelFlags /* channelsToProcess*/ ) override
    {
        if (m_dspFunctionBlockWiseNonReplacing != nullptr)
        {
            processBlockWiseNonReplacing (input, output);
            return;
        }

        if (m_dspFunctionBlockWiseReplacing != nullptr)
        {
            processBlockWiseReplacing (input, output);
            return;
        }

        if (m_dspFunctionSampleWise != nullptr)
        {
            processSampleWise (input, output);
            return;
        }

        HART_THROW_OR_RETURN_VOID (hart::NullPointerError, "DSP function is null");
    }

    void represent (std::ostream& stream) const override
    {
        stream << "DSPFunction (<function>, \"" << m_label << "\")";
    }

    void reset() override {}
    void setValue (int /*paramId*/, double /*value*/) override {}
    bool supportsChannelLayout (size_t /*inChannels*/, size_t /*outChannels*/) const override { return true; }
    bool supportsSampleRate (double /*sampleRateHz*/) const override { return true; }
    bool supportsEnvelopeFor (int /*paramId*/) const override { return false; }

private:
    void processSampleWise (const AudioBuffer<SampleType>& input, AudioBuffer<SampleType>& output)
    {
        const size_t numChannels = input.getNumChannels();
        const size_t numFrames = input.getNumFrames();

        hassert (numChannels == output.getNumChannels());

        for (size_t channel = 0; channel < numChannels; ++channel)
            for (size_t i = 0; i < numFrames; ++i)
                output[channel][i] = m_dspFunctionSampleWise (input[channel][i]);
    }

    void processBlockWiseReplacing (const AudioBuffer<SampleType>& input, AudioBuffer<SampleType>& output)
    {
        const size_t numFrames = input.getNumFrames();
        const size_t maxChannels = std::max (input.getNumChannels(), input.getNumChannels());
        m_inputOutputBuffer.setNumFrames (numFrames);

        for (size_t channel = 0; channel < input.getNumChannels(); ++channel)
            m_inputOutputBuffer.copyFrom (channel, 0, input, channel, 0, numFrames);

        for (size_t channel = input.getNumChannels(); channel < maxChannels; ++channel)
            m_inputOutputBuffer.clear (channel, 0, numFrames);

        const size_t originalNumChannels = m_inputOutputBuffer.getNumChannels();
        const size_t originalNumFrames = m_inputOutputBuffer.getNumFrames();
        const double originalSampleRateHz = m_inputOutputBuffer.getSampleRateHz();

        m_dspFunctionBlockWiseReplacing (m_inputOutputBuffer);

        if (m_inputOutputBuffer.getNumChannels() != originalNumChannels)
            HART_THROW_OR_RETURN_VOID (hart::ChannelLayoutError, "In-place DSP must not change the number of channels in the buffer");

        if (m_inputOutputBuffer.getNumFrames() != originalNumFrames)
            HART_THROW_OR_RETURN_VOID (hart::SizeError, "In-place DSP must not change the buffer length");

        if (!floatsEqual (m_inputOutputBuffer.getSampleRateHz(), originalSampleRateHz))
            HART_THROW_OR_RETURN_VOID (hart::SampleRateError, "In-place DSP must not change buffer's sample rate");

        for (size_t channel = 0; channel < output.getNumChannels(); ++channel)
            output.copyFrom (channel, 0, m_inputOutputBuffer, channel, 0, numFrames);
    }

    void processBlockWiseNonReplacing (const AudioBuffer<SampleType>& input, AudioBuffer<SampleType>& output)
    {
        const size_t originalNumChannels = output.getNumChannels();
        const size_t originalNumFrames = output.getNumFrames();
        const double originalSampleRateHz = output.getSampleRateHz();

        m_dspFunctionBlockWiseNonReplacing (input, output);

        // User's function is only allowed to modify the actual frames, and nothing else

        if (output.getNumFrames() != originalNumChannels)
            HART_THROW_OR_RETURN_VOID (hart::ChannelLayoutError, "DSP must not change number of channels in the output buffer");

        if (output.getNumFrames() != originalNumFrames)
            HART_THROW_OR_RETURN_VOID (hart::SizeError, "DSP must not change the output buffer length");

        if (!floatsEqual (output.getSampleRateHz(), originalSampleRateHz))
            HART_THROW_OR_RETURN_VOID (hart::SampleRateError, "DSP must not change sample rate of the output buffer");
    }

private:
    std::function<SampleType (SampleType)> m_dspFunctionSampleWise = nullptr;
    std::function<void (AudioBuffer<SampleType>&)> m_dspFunctionBlockWiseReplacing = nullptr;
    std::function<void (const AudioBuffer<SampleType>&, AudioBuffer<SampleType>&)> m_dspFunctionBlockWiseNonReplacing = nullptr;
    std::string m_label;
    AudioBuffer<SampleType> m_inputOutputBuffer;
};

HART_DSP_DECLARE_ALIASES_FOR (DSPFunction)

}  // namespace hart
