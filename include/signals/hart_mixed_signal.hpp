#pragma once

#include <memory>
#include <vector>

#include "signals/hart_signal.hpp"

namespace hart
{

/// @brief Produces a mix of multiple signals
/// @details You get this class as a result of adding or subtracting signals,
/// so you don't want to instantiate it directly.
/// @ingroup Signals
template<typename SampleType>
class MixedSignal:
    public Signal<SampleType, MixedSignal<SampleType>>
{
public:
    /// @brief Default constructor
    MixedSignal() = default;

    /// @brief Destructor
    ~MixedSignal() = default;

    /// @brief Copies other MixedSignal
    MixedSignal (const MixedSignal& other):
        Signal<SampleType, MixedSignal<SampleType>> (other)
    {
        m_signals.reserve (other.m_signals.size());

        for (const auto& src : other.m_signals)
            m_signals.emplace_back (src->copy());
    }

    /// @brief Moves from other MixedSignal
    MixedSignal (MixedSignal&& other) noexcept:
        Signal<SampleType, MixedSignal<SampleType>> (std::move (other)),
        m_signals (std::move (other.m_signals))
    {
    }

    /// @brief Copies other MixedSignal
    MixedSignal& operator= (const MixedSignal& other)
    {
        if (this == &other)
            return *this;

        // Copy base + its dspChain
        Signal<SampleType, MixedSignal<SampleType>>::operator=(other);

        // Deep-copy owned signals
        m_signals.clear();
        m_signals.reserve (other.m_signals.size());

        for (const auto& signal : other.m_signals)
            m_signals.emplace_back (signal->copy());

        return *this;
    }

    /// @brief Moves from other MixedSignal
    MixedSignal& operator=(MixedSignal&& other) noexcept
    {
        // Move base + its dspChain
        Signal<SampleType, MixedSignal<SampleType>>::operator= (std::move (other));

        // Move owned signals
        m_signals = std::move (other.m_signals);

        return *this;
    }

    // TODO: Move and smart pointer transfer

    /// @brief Adds a signal to a mix
    /// @details For internal use by "+" and "-" operators,
    /// @param signal Signal to add to the mix
    /// so you probably don't want to call it directly.
    void add (const SignalBase<SampleType>& signal)
    {
        m_signals.push_back (signal.copy());
    }

    void prepare (double sampleRateHz, size_t numOutputChannels, size_t maxBlockSizeFrames) override
    {
        for (auto& signal : m_signals)
            signal->prepareWithDSPChain (sampleRateHz, numOutputChannels, maxBlockSizeFrames);
    }

    void renderNextBlock (AudioBuffer<SampleType>& output) override
    {
        if (m_signals.empty())
        {
            output.clear();
            return;
        }

        // Render first signal
        m_signals[0]->renderNextBlockWithDSPChain (output);

        // Add all subsequent ones
        // TODO: Avoid memory allocation in this method, move it to prepare() and resize when needed
        AudioBuffer<SampleType> buffer = AudioBuffer<SampleType>::emptyLike (output);

        for (size_t i = 1; i < m_signals.size(); ++i)
        {
            m_signals[i]->renderNextBlockWithDSPChain (buffer);

            // TODO: implement AudioBuffer.addFrom()
            for (size_t channel = 0; channel < output.getNumChannels(); ++channel)
                for (size_t frame = 0; frame < output.getNumFrames(); ++frame)
                    output[channel][frame] += buffer[channel][frame];
        }
    }

    void reset() override
    {
        for (auto& signal : m_signals)
            signal->resetWithDSPChain();
    }

    void represent (std::ostream& s) const override
    {
        s << "<MixedSignal: {";

        for (size_t i = 0; i < m_signals.size(); ++i)
        {
            if (i > 0)
                s << ", ";

            m_signals[i]->representWithDSPChain (s);
        }

        s << "}>";
    }

private:
    std::vector<std::unique_ptr<SignalBase<SampleType>>> m_signals;
};

/// @brief Adds one signal to another, resulting in a new mixed signal
/// @ingroup Signals
/// @relates Signal
template<typename SampleType, typename DerivedSignalTypeLHS, typename DerivedSignalTypeRHS>
MixedSignal<SampleType> operator+ (
    const Signal<SampleType, DerivedSignalTypeLHS>& lhs,
    const Signal<SampleType, DerivedSignalTypeRHS>& rhs
    )
{
    MixedSignal<SampleType> mix;
    mix.add (static_cast<const DerivedSignalTypeLHS&> (lhs));
    mix.add (static_cast<const DerivedSignalTypeRHS&> (rhs));
    return mix;
}

/// @brief Subtracts one signal from another, resulting in a new mixed signal
/// @ingroup Signals
/// @relates Signal
template<typename SampleType, typename DerivedSignalTypeLHS, typename DerivedSignalTypeRHS>
MixedSignal<SampleType> operator- (
    const Signal<SampleType, DerivedSignalTypeLHS>& lhs,
    const Signal<SampleType, DerivedSignalTypeRHS>& rhs
    )
{
    MixedSignal<SampleType> mix;
    mix.add (static_cast<const DerivedSignalTypeLHS&> (lhs));
    mix.add (-static_cast<const DerivedSignalTypeRHS&> (rhs));
    return mix;
}

}  // namespace hart
