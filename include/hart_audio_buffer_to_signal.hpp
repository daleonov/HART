#pragma once

namespace hart
{

template<typename SampleType>
AudioBufferSignal<SampleType> AudioBuffer<SampleType>::toSignal (Loop loop) const &
{
    return AudioBufferSignal<SampleType> (*this, loop);
}

template<typename SampleType>
AudioBufferSignal<SampleType> AudioBuffer<SampleType>::toSignal (Loop loop) &&
{
    return AudioBufferSignal<SampleType> (std::move (*this), loop);
}

} // namespace hart
