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

template <typename SampleType>
class DSPSequence :
    public hart::DSP<SampleType, DSPSequence<SampleType>>
{
public:
    DSPSequence (std::vector<std::unique_ptr<DSPBase<SampleType>>> dspChain) :
        m_dspChain (std::move (dspChain))
    {
    }

    ~DSPSequence() override = default;
    DSPSequence (const DSPSequence&) = delete;
    DSPSequence (DSPSequence&&) noexcept = default;
    DSPSequence& operator= (const DSPSequence&) = delete;
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
            // TODO: Bypass - copy input to the output
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

    size_t size() const noexcept
    {
        return m_dspChain.size();
    }

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

    void append (std::unique_ptr<DSPBase<SampleType>> dsp)
    {
        if (dsp == nullptr)
            HART_THROW_OR_RETURN_VOID (hart::NullPointerError, "Appending nullptr DSP is not allowed");

        m_dspChain.emplace_back (std::move (dsp));
    }

    DSPBase<SampleType>* operator[] (long long int index)
    {
        return m_dspChain.at (normalizeIndex (index)).get();
    }

    const DSPBase<SampleType>* operator[] (long long int index) const
    {
        return m_dspChain.at (normalizeIndex (index)).get();
    }

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

HART_DSP_DECLARE_ALIASES_FOR (DSPSequenceBuilder);

#define HART_DSP_SEQUENCE(...) \
    (DSPSequenceBuilder() >> __VA_ARGS__).build()

#define HART_DSP_SEQUENCE_T(SampleType, ...) \
    (hart::DSPSequenceBuilder<SampleType>() >> __VA_ARGS__).build()

} // namespace hart
