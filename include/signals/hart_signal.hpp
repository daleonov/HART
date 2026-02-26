#pragma once

#include <algorithm>
#include <cmath>  // sin()
#include <cstdint>
#include <memory>
#include <random>
#include <string>
#include <vector>

#include "hart_audio_buffer.hpp"
#include "dsp/hart_dsp.hpp"
#include "hart_exceptions.hpp"
#include "hart_utils.hpp"  // floatsNotEqual(), roundToSizeT()

/// @defgroup Signals Signals
/// @brief Generate signals

namespace hart {

/// @brief Polymorphic base for all signals
/// @warning This class exists only for type erasure and polymorphism.
/// Do NOT inherit custom signals from this class directly.
/// Inherit from @ref hart::Signal instead.
/// @ingroup Signals
template<typename SampleType>
class SignalBase
{
public:
    /// @brief Default constructor
    SignalBase() = default;

    /// @brief Copies other signal
    SignalBase (const SignalBase& other):
        m_numChannels(other.m_numChannels),
        m_startTimestampSeconds (other.m_startTimestampSeconds)
    {
        if (other.m_dspChain.size() == 0)
            return;

        m_dspChain.reserve (other.m_dspChain.size());

        for (auto& dsp : other.m_dspChain)
        {
            std::unique_ptr<DSPBase<SampleType>> dspCopy = dsp->copy();

            // If you hit this, one of the effects in the DSP chain is either:
            // - non-copyable by design, or
            // - missing copy() implementation.
            // There's a good chance it's your custom DSP wrapper, as they're non-copyable by default,
            // while built-in HART DSP effects are properly copyable. If you really want your effect in
            // this DSP chain, consider one of the following options:
            // 1. Move the whole Signal instance that contains this DSP, instead of copying it.
            // 2. Move your DSP instance into the signal chain, instead of copying it. You can do this
            //    either via an rvalue ref, or a unique_ptr.
            // 3. Implement copy semantics (including copy() method) in your DSP subclass.
            if (dspCopy == nullptr)
                HART_THROW_OR_CONTINUE (hart::NullPointerError, "One of the DSP effects in the DSP chain has failed to return a copy");

            m_dspChain.push_back (std::move (dspCopy));
        }
    }

    /// @brief Moves from other signal
    SignalBase (SignalBase&& other) noexcept:
        m_numChannels (other.m_numChannels),
        m_startTimestampSeconds (other.m_startTimestampSeconds),
        m_dspChain (std::move (other.m_dspChain))
    {
        other.m_numChannels = 0;
        other.m_startTimestampSeconds = 0.0;
    }

    /// @brief Destructor
    virtual ~SignalBase() = default;

    /// @brief Copies from other signal
    SignalBase& operator= (const SignalBase& other)
    {
        if (this == &other)
            return *this;

        m_numChannels = other.m_numChannels;
        m_startTimestampSeconds = other.m_startTimestampSeconds;
        m_dspChain.clear();

        if (other.m_dspChain.size() == 0)
            return *this;

        m_dspChain.reserve (other.m_dspChain.size());

        for (auto& dsp : other.m_dspChain)
        {
            std::unique_ptr<DSPBase<SampleType>> dspCopy = dsp->copy();

            // If you hit this, one of the effects in the DSP chain is either:
            // - non-copyable by design, or
            // - missing copy() implementation.
            // There's a good chance it's your custom DSP wrapper, as they're non-copyable by default,
            // while built-in HART DSP effects are properly copyable. If you really want your effect in
            // this DSP chain, consider one of the following options:
            // 1. Move the whole Signal instance that contains this DSP, instead of copying it.
            // 2. Move your DSP instance into the signal chain, instead of copying it. You can do this
            //    either via an rvalue ref, or a unique_ptr.
            // 3. Implement copy semantics (including copy() method) in your DSP subclass.
            if (dspCopy == nullptr)
                HART_THROW_OR_CONTINUE (hart::NullPointerError, "One of the DSP effects in the DSP chain has failed to return a copy");

            m_dspChain.push_back (std::move (dspCopy));
        }

        return *this;
    }

    /// @brief Moves from other signal
    SignalBase& operator= (SignalBase&& other) noexcept
    {
        if (this == &other)
            return *this;

        m_numChannels = other.m_numChannels;
        m_dspChain = std::move (other.m_dspChain);
        m_startTimestampSeconds = other.m_startTimestampSeconds;
        other.m_numChannels = 0;
        other.m_startTimestampSeconds = 0.0;

        return *this;
    }

    /// @brief Tells the host whether this Signal is capable of generating audio for a certain amount of channels
    /// @details It is guaranteed that the signal will not receive unsupported number of channels in @ref renderNextBlock().
    /// This method is guaranteed to be called at least once before @ref prepare()
    /// @note This method should only care about the Signal itself, and not the attached effects in DSP chain - they'll be queried separately
    /// @param numChannels Number of output channels that will need to be filled
    /// @return true if signal is capable of filling this many channels with audio, false otherwise
    virtual bool supportsNumChannels (size_t /* numChannels */) const { return true; };

    /// @brief Tells whether this Signal supports given sample rate
    /// @details It is guaranteed to be called before @ref prepare()
    /// @note This method should only care about the Signal itself, and not the attached effects in DSP chain - they'll be queried separately
    /// @param sampleRateHz sample rate at which the audio should be generated
    /// @return true if signal is capable of generating audio at a given sample rate, false otherwise
    virtual bool supportsSampleRate (double /* sampleRateHz */) const { return true; }

    /// @brief Prepare the signal for rendering 
    /// @details This method is guaranteed to be called after @ref supportsNumChannels() and supportsSampleRate(),
    /// but before @ref renderNextBlock().
    /// It is guaranteed that ```numChannels``` obeys supportsNumChannels() preferences, same with ```sampleRateHz```
    /// and @ref supportsSampleRate(). It is guaranteed that all subsequent renderNextBlock() calls will be in line
    /// with the arguments received in this callback.
    /// @param sampleRateHz sample rate at which the audio should be generated
    /// @param numOutputChannels Number of output channels to be filled
    /// @param maxBlockSizeFrames Maximum block size in frames (samples)
    virtual void prepare (double sampleRateHz, size_t numOutputChannels, size_t maxBlockSizeFrames) = 0;

    /// @brief Renders next block audio for the signal
    /// @details Depending on circumstances, this callback will either be called once to generate an entire piece of audio from
    /// start to finish, or called repeatedly, one block at a time.
    /// This method is guaranteed to be called strictly after @ref prepare(), or not called at all.
    /// Number of channels and max block size are guaranteed to be in line with the ones set by prepare() callback.
    /// Assume sample rate to always be equal to the one received in the last @ref prepare() callback.
    /// All audio blocks except the last one are guaranteed to be equal to ```maxBlockSizeFrames``` set in @ref prepare() callback.
    /// @warning Remember that the very last block of audio is almost always smaller than the block size set in @ref prepare(), so be
    /// careful with buffer bounds.
    /// @note Note that this method does not have to be real-time safe, as all rendering always happens offline.
    /// Also note that, unlike real-time audio applications, this method is called on the same thread as all others like @ref prepare().
    /// @param output Output audio block
    /// @warning Output audio buffer is not guaranteed to be pre-filled with zeros, it may contain junk data.
    virtual void renderNextBlock (AudioBuffer<SampleType>& output) = 0;

    /// @brief Resets the Signal to initial state
    /// @details Ideally should be implemented in a way that audio produced after resetting is identical to audio produced after instantiation
    virtual void reset() = 0;

    /// @brief Returns a smart pointer with a copy of this object
    virtual std::unique_ptr<SignalBase<SampleType>> copy() const = 0;

    /// @brief Returns a smart pointer with a moved instance of this object
    virtual std::unique_ptr<SignalBase<SampleType>> move() = 0;

    /// @brief Makes a text representation of this Signal for test failure outputs.
    /// @details It is strongly encouraged to follow python's
    /// <a href="https://docs.python.org/3/reference/datamodel.html#object.__repr__" target="_blank">repr()</a>
    /// conventions for returned text - basically, put something like "MyClass(value1, value2)" (with no quotes)
    /// into the stream whenever possible, or "<Readable info in angled brackets>" otherwise.
    /// Also, use built-in stream manipulators like @ref dbPrecision wherever applicable.
    /// Use @ref HART_DEFINE_GENERIC_REPRESENT() to get a basic implementation for this method.
    /// @param[out] stream Output stream to write to
    virtual void represent (std::ostream& stream) const = 0;

    /// @brief Prepares the signal and all attached effects in the DSP chain for rendering
    /// @details This method is intended to be called by Signal hosts like AudioTestBuilder or Matcher.
    /// If you're making something that owns an instance of a Signal and needs it to generate audio,
    /// like a custom Matcher, you must call this method before calling @ref renderNextBlockWithDSPChain().
    /// You must also call @ref supportsNumChannels() and @ref supportsSampleRate() before calling this method.
    /// @attention If you're not making a custom host, you probably don't need to call this method.
    void prepareWithDSPChain (double sampleRateHz, size_t numOutputChannels, size_t maxBlockSizeFrames)
    {
        prepare (sampleRateHz, numOutputChannels, maxBlockSizeFrames);
        const size_t numInputChannels = numOutputChannels;

        for (auto& dsp : m_dspChain)
        {
            if (! dsp->supportsChannelLayout (numInputChannels, numOutputChannels))
                HART_THROW_OR_RETURN_VOID (ChannelLayoutError, "Not all DSP in the Signal's DSP chain support its channel layout");

            if (! dsp->supportsSampleRate (sampleRateHz))
                HART_THROW_OR_RETURN_VOID (hart::SampleRateError, "Not all DSP in the Signal's DSP chain support its sample rate");

            dsp->prepareWithEnvelopes (sampleRateHz, numInputChannels, numOutputChannels, maxBlockSizeFrames);
        }

        // Perform optional fast-forward set by DSP::skipTo()
        if (floatsNotEqual (m_startTimestampSeconds, 0.0))
            performSkipTo (sampleRateHz, numOutputChannels, maxBlockSizeFrames);
    }

    /// @brief Renders next block audio for the signal and all the effects in the DSP chain
    /// @details This method is intended to be called by Signal hosts like AudioTestBuilder or Matcher
    /// If you're making something that owns an instance of a Signal and needs it to generate audio,
    /// like a custom Matcher, you must call it after calling @ref prepareWithDSPChain().
    /// @attention If you're not making a custom host, you probably don't need to call this method.
    void renderNextBlockWithDSPChain (AudioBuffer<SampleType>& output)
    {
        renderNextBlock (output);
        AudioBuffer<SampleType>& inputReplacing = output;

        for (auto& dsp : m_dspChain)
            dsp->processWithEnvelopes (inputReplacing, output);
    }

    /// @brief Resets to Signal and all the effects attached to its DSP chain to initial state
    /// @details This method is intended to be called by hosts like AudioTestBuilder or Matcher.
    /// If you're not making a custom host, you probably don't need this method.
    virtual void resetWithDSPChain()
    {
        reset();

        for (auto& dsp : m_dspChain)
            dsp->reset();
    }

    /// @brief Makes a text representation of this signal and its entire signal chain for test failure outputs.
    /// @details Used by "<<" operator
    /// @private
    /// @param[out] stream Output stream to write to
    void representWithDSPChain (std::ostream& stream) const
    {
        represent (stream);

        for (const auto& dsp : m_dspChain)
            stream << " >> " << *dsp;
    }

    /// @brief Helper for template resolution
    /// @private
    using m_SampleType = SampleType;

protected:
    void setNumChannels (size_t numChannels)
    {
        m_numChannels = numChannels;
    }

    size_t getNumChannels()
    {
        return m_numChannels;
    }

    size_t m_numChannels = 1;
    double m_startTimestampSeconds = 0.0;
    std::vector<std::unique_ptr<DSPBase<SampleType>>> m_dspChain;

private:
    void performSkipTo (double sampleRateHz, size_t numOutputChannels, size_t maxBlockSizeFrames)
    {
        hassert (m_startTimestampSeconds > 0.0);

        size_t fastForwardFramesLeft = roundToSizeT (m_startTimestampSeconds * sampleRateHz);

        while (fastForwardFramesLeft != 0)
        {
            const size_t blockSizeFrames = fastForwardFramesLeft >= maxBlockSizeFrames ? maxBlockSizeFrames : fastForwardFramesLeft;
            AudioBuffer<SampleType> dummyAudioBlock (numOutputChannels, blockSizeFrames);
            renderNextBlockWithDSPChain (dummyAudioBlock);
            fastForwardFramesLeft -= blockSizeFrames;
        }

        m_startTimestampSeconds = 0.0;
    }
};

/// @brief Base class for signals
/// @ingroup Signals
/// @tparam SampleType Type of values that will be generated, typically ```float``` or ```double```
/// @tparam DerivedSignal Subclass for CRTP
template<typename SampleType, typename DerivedSignal>
class Signal:
    public SignalBase<SampleType>
{
public:
    /// @brief Adds a DSP effect to the end of signal's DSP chain
    /// @note You can only add a DSP by moving it, since some of the custom DSP wrappers can be non-copyable.
    /// If you do want to copy a dsp to the chain, use its DSPBase::copy() method explicitly.
    /// @param dsp A DSP effect instance
    template <typename DerivedDSP>
    typename std::enable_if<
        ! std::is_lvalue_reference<DerivedDSP>::value && std::is_base_of<DSPBase<SampleType>, typename std::decay<DerivedDSP>::type>::value,
        DerivedSignal&
    >::type
    followedBy (DerivedDSP&& dsp) &
    {
        _followedBy (std::forward<DerivedDSP> (dsp));
        return static_cast<DerivedSignal&> (*this);
    }

    /// @brief Adds a DSP effect to the end of signal's DSP chain
    /// @note You can only add a DSP by moving it, since some of the custom DSP wrappers can be non-copyable.
    /// If you do want to copy a dsp to the chain, use its DSPBase::copy() method explicitly.
    /// @param dsp A DSP effect instance
    template <typename DerivedDSP>
    typename std::enable_if<
        ! std::is_lvalue_reference<DerivedDSP>::value && std::is_base_of<DSPBase<SampleType>, typename std::decay<DerivedDSP>::type>::value,
        DerivedSignal&&
    >::type
    followedBy (DerivedDSP&& dsp) &&
    {
        _followedBy (std::forward<DerivedDSP> (dsp));
        return static_cast<DerivedSignal&&> (*this);
    }

    /// @brief Adds a DSP effect to the end of signal's DSP chain
    /// @note DSP smart pointer ownership will be transferred to the Signal instance
    /// @param dsp Smart pointer to DSP effect instance
    DerivedSignal& followedBy (std::unique_ptr<DSPBase<SampleType>> dsp) &
    {
        _followedBy (std::move (dsp));
        return static_cast<DerivedSignal&> (*this);
    }

    /// @brief Adds a DSP effect to the end of signal's DSP chain
    /// @note DSP smart pointer ownership will be transferred to the Signal instance
    /// @param dsp Smart pointer to DSP effect instance
    DerivedSignal&& followedBy (std::unique_ptr<DSPBase<SampleType>> dsp) &&
    {
        _followedBy (std::move (dsp));
        return static_cast<DerivedSignal&&> (*this);
    }

    std::unique_ptr<SignalBase<SampleType>> copy() const override
    {
        return hart::make_unique<DerivedSignal> (static_cast<const DerivedSignal&> (*this));
    }

    std::unique_ptr<SignalBase<SampleType>> move() override
    {
        return hart::make_unique<DerivedSignal> (std::move (static_cast<DerivedSignal&> (*this)));
    }

    /// @brief Skips the signal to a specific timestamp
    /// @details Fast-forwards the signal, with all attaches DSP effects and their automation
    /// envelopes. Calling it multiple times on one instance will stack the skip times.
    /// @note Keep in mind that the skip is accurate within one audio frame tolerance
    /// @param startTimestampSeconds How much time to skip from the start of the signal
    DerivedSignal& skipTo (double startTimestampSeconds) &
    {
        _skipTo (startTimestampSeconds);
        return static_cast<DerivedSignal&> (*this);
    }

    /// @brief Skips the signal to a specific timestamp
    /// @details Fast-forwards the signal, with all attaches DSP effects and their automation
    /// envelopes. Calling it multiple times on one instance will stack the skip times.
    /// @note Keep in mind that the skip is accurate within one audio frame tolerance
    /// @param startTimestampSeconds How much time to skip from the start of the signal
    DerivedSignal&& skipTo (double startTimestampSeconds) &&
    {
        _skipTo (startTimestampSeconds);
        return static_cast<DerivedSignal&&> (*this);
    }

    /// @brief Returns a copy of this signal, but with flipped phase
    DerivedSignal operator-() const
    {
        auto newSignal = static_cast<const DerivedSignal&> (*this);
        newSignal.m_dspChain.emplace_back (hart::make_unique<GainLinear<SampleType>> (SampleType (-1)));
        return newSignal;
    }

    /// @brief Returns a copy of this signal, but with flipped phase
    DerivedSignal operator~() const
    {
        return -(*this);
    }

private:
    void _skipTo (double startTimestampSeconds)
    {
        if (startTimestampSeconds < 0)
            HART_THROW_OR_RETURN (hart::ValueError, "Can't skip to a negative timestamp", static_cast<DerivedSignal&> (*this));

        this->m_startTimestampSeconds += startTimestampSeconds;
    }

    template <typename DerivedDSP>
    void _followedBy (DerivedDSP&& dsp)
    {
        static_assert (!std::is_lvalue_reference<DerivedDSP>::value, "DSP must be passed as an rvalue");
        static_assert (std::is_base_of<DSPBase<SampleType>, typename std::decay<DerivedDSP>::type>::value, "Argument must be a DSP object");
        this->m_dspChain.emplace_back (dsp.move());
    }

    void _followedBy(std::unique_ptr<DSPBase<SampleType>> dsp)
    {
        this->m_dspChain.emplace_back (std::move (dsp));
    }
};

/// @brief Prints readable text representation of the Signal object into the I/O stream
/// @relates Signal
/// @ingroup Signals
template<typename SampleType>
std::ostream& operator<< (std::ostream& stream, const SignalBase<SampleType>& signal)
{
    signal.representWithDSPChain (stream);
    return stream;
}

/// @brief Adds a DSP effect to the end of signal's DSP chain by transfering it
/// @details Accepts any value that can be moved into the signal's DSP chain:
/// - An rvalue of a class derived from `DSPBase<SampleType>`
/// - An `std::unique_ptr<DSPBase<SampleType>>`
/// @relates Signal
/// @ingroup Signals
template<typename DerivedSignal, typename DerivedDSP>
auto operator>> (DerivedSignal& signal, DerivedDSP&& dsp) -> decltype (signal.followedBy (std::forward<DerivedDSP> (dsp)))
{
    return signal.followedBy (std::forward<DerivedDSP> (dsp));
}

/// @brief Adds a DSP effect to the end of signal's DSP chain by transfering it
/// @details Accepts any value that can be moved into the signal's DSP chain:
/// - An rvalue of a class derived from `DSPBase<SampleType>`
/// - An `std::unique_ptr<DSPBase<SampleType>>`
/// @relates Signal
/// @ingroup Signals
template<typename DerivedSignal, typename DerivedDSP>
auto operator>> (DerivedSignal&& signal, DerivedDSP&& dsp) -> decltype (std::move (signal).followedBy (std::forward<DerivedDSP> (dsp)))
{
    return std::move (signal).followedBy (std::forward<DerivedDSP> (dsp));
}

}  // namespace hart

/// @brief Forbids @ref hart::Signal::copy() and @ref hart::Signal::move() methods
/// @details Put this into your class body's ```public``` section if either is true:
///  - Your class is not trivially copyable and movable
///  - You don't want to trouble yourself with implementing move and copy semantics for your class
///
/// Otherwise, use @ref HART_SIGNAL_DEFINE_COPY_AND_MOVE() instead.
/// Obviously, you won't be able to pass your class to the host
/// by reference, copy or explicit move, but you still can pass
/// it wrapped into a smart pointer like so:
/// ```cpp
/// processAudioWith (MyDSP())
///    .withInputSignal(hart::make_unique<MyDspType>())  // As input signal
///    .expectTrue (EqualsTo (hart::make_unique<MyDspType>()))  // As reference signal
///    .process();
/// ```
/// @ingroup Signals
#define HART_SIGNAL_FORBID_COPY_AND_MOVE \
    std::unique_ptr<Signal<SampleType>> copy() const override { \
        static_assert(false, "This Signal cannot be copied"); \
        return nullptr; \
    } \
    std::unique_ptr<Signal<SampleType>> move() override { \
        static_assert(false, "This Signal cannot be moved"); \
        return nullptr; \
    }

/// @private
#define HART_SIGNAL_DECLARE_ALIASES_FOR(ClassName) \
    namespace aliases_float{\
        using ClassName = hart::ClassName<float>;\
    }\
    namespace aliases_double{\
        using ClassName = hart::ClassName<double>;\
    }
