#pragma once

namespace hart
{

/// @brief Creates a HART audio buffer from its JUCE counterpart
/// @details This is a part of the interoperability layer with JUCE framework.
/// For all JUCE related methods, see: `fromJuceAudioBuffer()`, `fromJuceAudioBlock()`,
/// `copyFromJuceAudioBuffer()`, `copyFromJuceAudioBlock()`, `toJuceAudioBufferView()`,
/// `toJuceAudioBufferCopy()`, `toJuceAudioBlock()`.
/// @tparam JuceAudioBufferType Should resolve to `juce::AudioBuffer<SampleType>`
/// @param juceAudioBuffer JUCE buffer to copy data from
/// @param sampleRateHz Sample rate value to put in the resulting HART buffer's metadata.
/// Optional, defults to `NaN`.
/// @return A HART audio buffer with same data as its JUCE counterpart
template <typename SampleType>
template <typename JuceAudioBufferType>
AudioBuffer<SampleType> AudioBuffer<SampleType>::fromJuceAudioBuffer (const JuceAudioBufferType& juceAudioBuffer, double sampleRateHz)
{
    const size_t numFrames = static_cast<size_t> (juceAudioBuffer.getNumSamples());
    const size_t numChannels = static_cast<size_t> (juceAudioBuffer.getNumChannels());
    AudioBuffer<SampleType> hartAudioBuffer (numChannels, numFrames, sampleRateHz);
    hartAudioBuffer.copyFromJuceAudioBuffer (juceAudioBuffer);
    return hartAudioBuffer;
}

/// @brief Creates a HART audio buffer from JUCE's AudioBlock view
/// @details This is a part of the interoperability layer with JUCE framework.
/// For all JUCE related methods, see: `fromJuceAudioBuffer()`, `fromJuceAudioBlock()`,
/// `copyFromJuceAudioBuffer()`, `copyFromJuceAudioBlock()`, `toJuceAudioBufferView()`,
/// `toJuceAudioBufferCopy()`, `toJuceAudioBlock()`.
/// @tparam JuceAudioBlockType Should resolve to `juce::AudioBlock<SampleType>` or `juce::AudioBlock<const SampleType>`
/// @param juceAudioBlock JUCE audio block to copy data from
/// @param sampleRateHz Sample rate value to put in the resulting HART buffer's metadata.
/// Optional, defults to `NaN`.
/// @return A HART audio buffer with same data as whatever JUCE audio block points to
template <typename SampleType>
template <typename JuceAudioBlockType>
AudioBuffer<SampleType> AudioBuffer<SampleType>::fromJuceAudioBlock (const JuceAudioBlockType& juceAudioBlock, double sampleRateHz)
{
    const size_t numFrames = static_cast<size_t> (juceAudioBlock.getNumSamples());
    const size_t numChannels = static_cast<size_t> (juceAudioBlock.getNumChannels());
    AudioBuffer<SampleType> hartAudioBuffer (numChannels, numFrames, sampleRateHz);
    hartAudioBuffer.copyFromJuceAudioBlock (juceAudioBlock);
    return hartAudioBuffer;
}

/// @brief Copies data from a JUCE `AudioBuffer`
/// @details This is a part of the interoperability layer with JUCE framework.
/// For all JUCE related methods, see: `fromJuceAudioBuffer()`, `fromJuceAudioBlock()`,
/// `copyFromJuceAudioBuffer()`, `copyFromJuceAudioBlock()`, `toJuceAudioBufferView()`,
/// `toJuceAudioBufferCopy()`, `toJuceAudioBlock()`.
/// @tparam JuceAudioBufferType Should resolve to `juce::AudioBuffer<SampleType>`
/// @param juceAudioBuffer JUCE AudioBuffer to copy data from
template <typename SampleType>
template <typename JuceAudioBufferType>
void AudioBuffer<SampleType>::copyFromJuceAudioBuffer (const JuceAudioBufferType& juceAudioBuffer)
{
    const size_t numFrames = static_cast<size_t> (juceAudioBuffer.getNumSamples());
    const size_t numChannels = static_cast<size_t> (juceAudioBuffer.getNumChannels());

    if (numFrames != getNumFrames())
        HART_THROW_OR_RETURN_VOID (SizeError, "Number of frames mismatch");

    if (numChannels != getNumChannels())
        HART_THROW_OR_RETURN_VOID (ChannelLayoutError, "Number of channels mismatch");

    for (const size_t channel = 0; channel < numChannels; ++channel)
    {
        copyFrom (
            channel,
            0,  // destStartFrame
            juceAudioBuffer.getReadPointer (static_cast<int> (channel)),
            numFrames
        );
    }
}

/// @brief Copies data from a JUCE `AudioBlock` view
/// @details This is a part of the interoperability layer with JUCE framework.
/// For all JUCE related methods, see: `fromJuceAudioBuffer()`, `fromJuceAudioBlock()`,
/// `copyFromJuceAudioBuffer()`, `copyFromJuceAudioBlock()`, `toJuceAudioBufferView()`,
/// `toJuceAudioBufferCopy()`, `toJuceAudioBlock()`.
/// @tparam JuceAudioBlockType Should resolve to `juce::AudioBlock<SampleType>` or `juce::AudioBlock<const SampleType>`
/// @param juceAudioBlock JUCE AudioBlock to copy data from
template <typename SampleType>
template <typename JuceAudioBlockType>
void AudioBuffer<SampleType>::copyFromJuceAudioBlock (const JuceAudioBlockType& juceAudioBlock)
{
    const size_t numFrames = static_cast<size_t> (juceAudioBlock.getNumSamples());
    const size_t numChannels = static_cast<size_t> (juceAudioBlock.getNumChannels());

    if (numFrames != getNumFrames())
        HART_THROW_OR_RETURN_VOID (SizeError, "Number of frames mismatch");

    if (numChannels != getNumChannels())
        HART_THROW_OR_RETURN_VOID (ChannelLayoutError, "Number of channels mismatch");

    for (const size_t channel = 0; channel < numChannels; ++channel)
    {
        copyFrom (
            channel,
            0,  // destStartFrame
            juceAudioBlock.getChannelPointer (channel),
            numFrames
        );
    }
}

/// @brief Creates a JUCE `AudioBuffer` with contents pointing to this buffer's data
/// @details This is a part of the interoperability layer with JUCE framework.
/// For all JUCE related methods, see: `fromJuceAudioBuffer()`, `fromJuceAudioBlock()`,
/// `copyFromJuceAudioBuffer()`, `copyFromJuceAudioBlock()`, `toJuceAudioBufferView()`,
/// `toJuceAudioBufferCopy()`, `toJuceAudioBlock()`.
/// @attention The resulting `juce::AudioBuffer` will not own the audio, and will
/// merely point to this `hart::AudioBuffer`'s data, being tied to its lifetime.
/// @tparam JuceAudioBufferType Your target type, e.g. `juce::AudioBuffer<float>`
/// @return A JUCE `AudioBuffer` mutable view of this HART buffer
template <typename SampleType>
template <typename JuceAudioBufferType>
JuceAudioBufferType AudioBuffer<SampleType>::toJuceAudioBufferView()
{
    return JuceAudioBufferType (
        getArrayOfWritePointers(),
        static_cast<int> (getNumChannels()),
        static_cast<int> (getNumFrames())
    );
}

/// @brief Creates a JUCE `AudioBuffer` with contents identical to this buffer
/// @details This is a part of the interoperability layer with JUCE framework.
/// For all JUCE related methods, see: `fromJuceAudioBuffer()`, `fromJuceAudioBlock()`,
/// `copyFromJuceAudioBuffer()`, `copyFromJuceAudioBlock()`, `toJuceAudioBufferView()`,
/// `toJuceAudioBufferCopy()`, `toJuceAudioBlock()`.
/// It will instantiate a `juce::AudioBuffer` with its own copied storage.
/// @note Use this only if you really need an independent copy of the buffer,
/// otherwise consider using `toJuceAudioBufferView()` to avoid expensive copying.
/// @tparam JuceAudioBufferType Your target type, e.g. `juce::AudioBuffer<float>`
/// @return A JUCE `AudioBuffer` copy of this HART buffer
template <typename SampleType>
template <typename JuceAudioBufferType>
JuceAudioBufferType AudioBuffer<SampleType>::toJuceAudioBufferCopy() const
{
    JuceAudioBufferType juceAudioBufferWithOwnStorage (
        static_cast<int> (getNumChannels()),
        static_cast<int> (getNumFrames())
    );

    for (size_t channel = 0; channel < getNumChannels(); ++channel)
    {
        std::copy (
            (*this)[channel],
            (*this)[channel] + getNumFrames(),
            juceAudioBufferWithOwnStorage.getWritePointer (static_cast<int> (channel))
        );
    }

    return juceAudioBufferWithOwnStorage;
}

/// @brief Creates a mutable JUCE `AudioBlock` that points to data this buffer
/// @details This is a part of the interoperability layer with JUCE framework.
/// For all JUCE related methods, see: `fromJuceAudioBuffer()`, `fromJuceAudioBlock()`,
/// `copyFromJuceAudioBuffer()`, `copyFromJuceAudioBlock()`, `toJuceAudioBufferView()`,
/// `toJuceAudioBufferCopy()`, `toJuceAudioBlock()`.
/// @tparam JuceAudioBlockType Your target type, e.g. `juce::AudioBlock<float>`
/// @return A JUCE `AudioBlock` mutable view, representing the data inside this HART buffer
template <typename SampleType>
template <typename JuceAudioBlockType>
JuceAudioBlockType AudioBuffer<SampleType>::toJuceAudioBlock()
{
    return JuceAudioBlockType (
        getArrayOfWritePointers(),
        getNumChannels(),
        getNumFrames()
    );
}

/// @brief Creates an immutable JUCE `AudioBlock` that points to data this buffer
/// @details This is a part of the interoperability layer with JUCE framework.
/// For all JUCE related methods, see: `fromJuceAudioBuffer()`, `fromJuceAudioBlock()`,
/// `copyFromJuceAudioBuffer()`, `copyFromJuceAudioBlock()`, `toJuceAudioBufferView()`,
/// `toJuceAudioBufferCopy()`, `toJuceAudioBlock()`.
/// @tparam JuceAudioBlockType Your target type, e.g. `juce::AudioBlock<const float>`
/// @return An immutable JUCE `AudioBlock` view, representing the data inside this HART buffer
template <typename SampleType>
template <typename JuceAudioBlockType>
JuceAudioBlockType AudioBuffer<SampleType>::toJuceAudioBlock() const
{
    return JuceAudioBlockType (
        getArrayOfReadPointers(),
        getNumChannels(),
        getNumFrames()
    );
}

}  // namespace hart
