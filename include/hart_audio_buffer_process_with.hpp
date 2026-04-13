#pragma once

#include "hart_exceptions.hpp"

namespace hart {

template <typename SampleType>
void AudioBuffer<SampleType>::_processWith (DSPBase<SampleType>& dsp, size_t requestedBlockSizeFrames, Preparation dspPreparation)
{
    if (getNumChannels() == 0)
        HART_THROW_OR_RETURN_VOID (hart::ChannelLayoutError, "Can't process an AudioBuffer with a DSP if the buffer is default-constructed or has no channels allocated");

    if (getNumFrames() == 0)
        HART_THROW_OR_RETURN_VOID (hart::SizeError, "Can't process an AudioBuffer with a DSP if the buffer is default-constructed or has no frames allocated");

    if (! hasSampleRate())
        HART_THROW_OR_RETURN_VOID (hart::SampleRateError, "Can't process an AudioBuffer with a DSP if the buffer's sample rate is undefined");

    if (! dsp.supportsChannelLayout (getNumChannels(), getNumChannels()))
        HART_THROW_OR_RETURN_VOID (hart::ChannelLayoutError, "Provided DSP doesn't support the number of channels that this buffer has");

    if (! dsp.supportsSampleRate (getSampleRateHz()))
        HART_THROW_OR_RETURN_VOID (hart::SampleRateError, "Provided DSP doesn't support the sample rate that this buffer has");

    const size_t blockSizeFrames = (requestedBlockSizeFrames == 0) ? getNumFrames() : requestedBlockSizeFrames;

    if (dspPreparation == Preparation::reset || dspPreparation == Preparation::resetAndPrepare)
        dsp.resetWithEnvelopes();

    if (dspPreparation == Preparation::prepare || dspPreparation == Preparation::resetAndPrepare)
        dsp.prepareWithEnvelopes (getSampleRateHz(), getNumChannels(), getNumChannels(), blockSizeFrames);

    if (blockSizeFrames >= getNumFrames())
    {
        // Render in one go
        dsp.processWithEnvelopes (*this, *this);
        return;
    }

    size_t offsetFrames = 0;
    hart::AudioBuffer<SampleType> block (getNumChannels(), blockSizeFrames, getSampleRateHz());
            
    while (offsetFrames < getNumFrames())
    {
        const size_t currentBlockSizeFrames = std::min (blockSizeFrames, getNumFrames() - offsetFrames);
        hassert (currentBlockSizeFrames <= blockSizeFrames);

        if (currentBlockSizeFrames != block.getNumFrames())
        {
            // Expecting partial block at the end
            hassert (currentBlockSizeFrames < blockSizeFrames);
            hassert (block.getNumFrames() == blockSizeFrames);

            block.setNumFrames (currentBlockSizeFrames);
        }

        hassert (getNumFrames() > block.getNumFrames());
        hassert (getNumChannels() == block.getNumChannels());

        for (size_t channel = 0; channel < getNumChannels(); ++channel)
            block.copyFrom (channel, 0, *this, channel, offsetFrames, currentBlockSizeFrames);

        dsp.processWithEnvelopes (block, block);

        for (size_t channel = 0; channel < getNumChannels(); ++channel)
            copyFrom (channel, offsetFrames, block, channel, 0, currentBlockSizeFrames);

        offsetFrames += currentBlockSizeFrames;
    }
}

} // namespace hart
