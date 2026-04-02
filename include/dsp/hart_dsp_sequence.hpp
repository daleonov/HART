#pragma once

#include <cstddef>  // ptrdiff_t
#include <memory>
#include <sstream>
#include <utility>
#include <vector>

#include "hart_dsp.hpp"
#include "hart_exceptions.hpp"
#include "hart_utils.hpp"  // make_unique()

namespace hart
{

/// @brief A DSP unit that renders audio through a linear sequence of DSP instances
/// @details It's a container-like DSP subclass that owns a linear chain of
/// other DSP units and applies them one after another.
/// The first DSP in the chain processes the input buffer into the output buffer,
/// while all following DSP units process the output buffer in-place.
/// Nested DSP sequences are fully supported, since `DSPSequence` itself is a
/// normal DSP subclass.
/// Empty sequences are also valid, and just output silence (zeros).
///
/// This class is intended to be constructed via the `HART_DSP_SEQUENCE()` macro:
/// @code
/// auto myDspChain = HART_DSP_SEQUENCE (
///     GainDb (-3_dB) >> HardClip (-1_dB) >> TimeShift (5_ms)
///     );
/// @endcode
/// but using the ctor directly is also appropriate in some cases, for example,
/// for creating an empty sequence, and then filling it using some loop.
///
/// The class also supports runtime mutation:
/// - adding DSP units
/// - removing (extracting) DSP units
/// - peeking at DSP units without extracting them
///
/// @note
/// Although the class can be constructed directly, the macro syntax is usually
/// more concise and preferred in tests.
/// @see `HART_DSP_SEQUENCE()`, `HART_DSP_SEQUENCE_T()`
/// @ingroup DSP
template <typename SampleType>
class DSPSequence :
    public hart::DSP<SampleType, DSPSequence<SampleType>>
{
public:
    /// @brief Creates a sequence of DSP processors
    /// @attention Consider using `HART_DSP_SEQUENCE()` or `HART_DSP_SEQUENCE_T()`
    /// instead of using this ctor directly, unless you intend to create an
    /// empty sequence.
    /// @param dspChain A vector with DSP objects. It will be moved into the DSPSequence,
    /// along with all DSP instances.
    DSPSequence (std::vector<std::unique_ptr<DSPBase<SampleType>>> dspChain) :
        m_dspChain (std::move (dspChain))
    {
    }

    /// @brief Destructor
    ~DSPSequence() override = default;

    /// @brief No copy construction
    /// @details Copy is still supported, but you have to call `copy()` method explicitly
    DSPSequence (const DSPSequence&) = delete;

    /// @brief Moves DSP chain from another DSP sequence
    DSPSequence (DSPSequence&&) noexcept = default;

    /// @brief No copy assignment
    /// @details Copy is still supported, but you have to call `copy()` method explicitly
    DSPSequence& operator= (const DSPSequence&) = delete;

    /// @brief Moves DSP chain from another DSP sequence
    DSPSequence& operator= (DSPSequence&&) noexcept = default;

    void prepare (double sampleRateHz, size_t numInputChannels, size_t numOutputChannels, size_t maxBlockSizeFrames) override
    {
        for (auto& dsp : m_dspChain)
            dsp->prepare (sampleRateHz, numInputChannels, numOutputChannels, maxBlockSizeFrames);
    }

    void process (const AudioBuffer<SampleType>& input, AudioBuffer<SampleType>& output, const EnvelopeBuffers& envelopeBuffers, ChannelFlags channelsToProcess) override
    {
        if (m_dspChain.empty())
        {
            // No DSP - no audio. Bye.
            // Bypass could also be appropriate, but too tricky with assymetrical channel layouts
            output.clear();
            return;
        }

        // First DSP - non-replacing processing
        m_dspChain[0]->process (input, output, envelopeBuffers, channelsToProcess);

        if (m_dspChain.size() == 1)
            return;

        // All following DSPs - replacing processing
        const AudioBuffer<SampleType>& inputReplacing = output;

        for (size_t i = 1; i < m_dspChain.size(); ++i)
            m_dspChain[i]->process (inputReplacing, output, envelopeBuffers, channelsToProcess);
    }

    bool supportsSampleRate (double sampleRate) const override
    {
        for (auto& dsp : m_dspChain)
            if (! dsp->supportsSampleRate (sampleRate))
                return false;

        return true;
    }

    bool supportsChannelLayout (size_t numInputChannels, size_t numOutputChannels) const override
    {
        for (auto& dsp : m_dspChain)
            if (! dsp->supportsChannelLayout (numInputChannels, numOutputChannels))
                return false;

        return true;
    }

    virtual bool supportsEnvelopeFor (int /* paramId */) const { return false; }
    virtual void setValue (int /* paramId */, double /* value */) {}

    void represent (std::ostream& stream) const override
    {
        stream << "HART_DSP_SEQUENCE (";

        for (size_t i = 0; i < m_dspChain.size(); ++i)
        {
            if (i != 0)
                stream << " >> ";

            m_dspChain[i]->represent (stream);
        }

        stream << ')';
    }

    std::unique_ptr<DSPBase<SampleType>> copy() const override
    {
        std::vector<std::unique_ptr<DSPBase<SampleType>>> dspCopies;
        dspCopies.reserve (m_dspChain.size());

        for (size_t i = 0; i < m_dspChain.size(); ++i)
        {
            auto dspCopy = m_dspChain[i]->copy();
            if (! dspCopy)
                HART_THROW_OR_RETURN (
                    hart::NullPointerError,
                    "DSPSequence::copy() failed because one of the contained DSP units is not copyable",
                    nullptr);

            dspCopies.push_back (std::move (dspCopy));
        }

        return hart::make_unique<DSPSequence> (std::move (dspCopies));
    }

    /// @brief Returns the number of DSP elements in the sequence
    /// @return Number of elements in the DSP chain
    size_t size() const noexcept
    {
        return m_dspChain.size();
    }

    /// @brief Appends an element to the end of DSP chain
    /// @details The element will be owned by the DSPSequence. You can get it back
    /// by using `pop()` method, or peek by using square brackets `[]` operator.
    /// @param dsp DSP element ot be appended. Must be a `hart::DSP` subclass.
    template<typename DerivedDSP,
        typename = typename std::enable_if<
            ! std::is_lvalue_reference<DerivedDSP>::value
            && std::is_base_of<
                DSPBase<SampleType>,
                typename std::decay<DerivedDSP>::type
                >::value
            >::type>
    void append (DerivedDSP&& dsp)
    {
        m_dspChain.emplace_back (dsp.move());
    }

    /// @brief Appends an element to the end of DSP chain
    /// @details The smart pointer will be owned by the DSPSequence. You can get it back
    /// by using `pop()` method, or peek by using square brackets `[]` operator.
    /// @param dsp Smart pointer with a DSP element ot be appended
    void append (std::unique_ptr<DSPBase<SampleType>> dsp)
    {
        if (dsp == nullptr)
            HART_THROW_OR_RETURN_VOID (hart::NullPointerError, "Appending nullptr DSP is not allowed");

        m_dspChain.emplace_back (std::move (dsp));
    }

    /// @brief Accesses a specific element in the DSP sequence (mutable version)
    /// @param index Index of the element. Can be negative. For non-negative values, it's a usual 0-based index.
    /// For negative values, it's counted from the end, e.g. "-1" is the last element, "-2" is the second to last etc.
    /// @throws hart::IndexError if the index is out of range
    /// @return A pointer to the DSP instance
    DSPBase<SampleType>* operator[] (long long int index)
    {
        return m_dspChain.at (normalizeIndex (index)).get();
    }

    /// @brief Accesses a specific element in the DSP sequence (immutable version)
    /// @param index Index of the element. Can be negative. For non-negative values, it's a usual 0-based index.
    /// For negative values, it's counted from the end, e.g. "-1" is the last element, "-2" is the second to last etc.
    /// @throws hart::IndexError if the index is out of range
    /// @return A pointer to the DSP instance
    const DSPBase<SampleType>* operator[] (long long int index) const
    {
        return m_dspChain.at (normalizeIndex (index)).get();
    }

    /// @brief Extracts a specific element in the DSP chain by removing it
    /// @param index Index of the element. Can be negative. For non-negative values, it's a usual 0-based index.
    /// For negative values, it's counted from the end, e.g. "-1" is the last element, "-2" is the second to last etc.
    /// @throws hart::IndexError if the index is out of range
    /// @return A smart pointer with the DSP instance
    std::unique_ptr<DSPBase<SampleType>> pop (long long int index = -1)
    {
        size_t normalizedIndex = normalizeIndex (index);
        auto iterator = m_dspChain.begin() + static_cast<std::ptrdiff_t> (normalizedIndex);

        std::unique_ptr<DSPBase<SampleType>> poppedDSP = std::move (*iterator);
        m_dspChain.erase (iterator);
        return poppedDSP;
    }

private:
    size_t normalizeIndex (long long int index) const
    {
        const long long int size = static_cast<long long int> (m_dspChain.size());
        const long long int normalizedIndex = index < 0 ? size + index : index;

        if (normalizedIndex < 0 || normalizedIndex >= size)
            HART_THROW_OR_RETURN (hart::IndexError, "DSPSequence index out of range", 0);

        return static_cast<size_t> (normalizedIndex);
    }

    std::vector<std::unique_ptr<DSPBase<SampleType>>> m_dspChain;
};

/// @brief Internal helper used by `HART_DSP_SEQUENCE()`
/// @details
/// This move-only builder accumulates DSP units via chained `>>` operators and
/// converts them into a `DSPSequence` via `build()`. Not intended to be used directly,
/// unless implementing advanced custom sequence-building logic.
/// @ingroup DSP
/// @private
template <typename SampleType>
class DSPSequenceBuilder
{
public:
    DSPSequenceBuilder() = default;
    DSPSequenceBuilder (const DSPSequenceBuilder&) = delete;
    DSPSequenceBuilder (DSPSequenceBuilder&&) noexcept = default;
    DSPSequenceBuilder& operator= (const DSPSequenceBuilder&) = delete;
    DSPSequenceBuilder& operator= (DSPSequenceBuilder&&) noexcept = default;
    ~DSPSequenceBuilder() = default;

    template <typename DerivedDSP>
    DSPSequenceBuilder&& operator>> (DerivedDSP&& dsp) &&
    {
        m_dspChain.emplace_back (dsp.move());
        return std::move (*this);
    }

    DSPSequenceBuilder&& operator>> (std::unique_ptr<DSPBase<SampleType>> dsp) &&
    {
        m_dspChain.emplace_back (std::move (dsp));
        return std::move (*this);
    }

    DSPSequence<SampleType> build()
    {
        return DSPSequence<SampleType> (std::move (m_dspChain));
    }

private:
    std::vector<std::unique_ptr<DSPBase<SampleType>>> m_dspChain;
};

HART_DSP_DECLARE_ALIASES_FOR (DSPSequence);
HART_DSP_DECLARE_ALIASES_FOR (DSPSequenceBuilder);

/// @brief Creates a linear DSP sequence using concise chain syntax
/// @details
/// This macro is the preferred way to create a `DSPSequence`.
///
/// It accepts a chain of DSP units chained with `>>`, preserving the exact
/// order in which they should process the audio.
///
/// Example:
/// @code
/// auto myDspChain = HART_DSP_SEQUENCE (
///     GainDb (-3_dB) >> HardClip (-1_dB) >> TimeShift (5_ms)
///     );
/// @endcode
///
/// The resulting object behaves as a normal DSP subclass and can, among other
/// things, be:
/// - passed into `processAudioWith()`
/// - used in a `Signal`'s DSP chain
/// - nested into another sequence
/// - modified by appending or popping DSP elements
/// - deep-copied via `copy()`
///
/// @note
/// This macro depends on HART DSP aliases being declared for the chosen sample
/// type, for example:
/// @code
/// HART_DECLARE_ALIASES_FOR_FLOAT;
/// @endcode
/// (or a similar macro for `double`).
/// You can also just use explicitly typed`HART_DSP_SEQUENCE_T()`.
/// @ingroup DSP
#define HART_DSP_SEQUENCE(...) \
    (DSPSequenceBuilder() >> __VA_ARGS__).build()

/// @brief Creates a DSP sequence for an explicit sample type
/// @details
/// This typed variant of `HART_DSP_SEQUENCE(...)` is useful when DSP aliases
/// are not declared.
///
/// Example:
/// @code
/// auto chain = HART_DSP_SEQUENCE_T (
///     float,
///     GainDb<float> (-3_dB) >> HardClip<float> (-1_dB)
///     );
/// @endcode
///
/// @param SampleType The sample type used by the sequence and all DSP units
/// @ingroup DSP
#define HART_DSP_SEQUENCE_T(SampleType, ...) \
    (hart::DSPSequenceBuilder<SampleType>() >> __VA_ARGS__).build()

} // namespace hart
