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
        if (other.dspChain.size() == 0)
            return;

        dspChain.reserve (dspChain.size());

        for (auto& dsp : other.dspChain)
            dspChain.push_back (dsp->copy());
    }

    /// @brief Moves from other signal
    SignalBase (SignalBase&& other) noexcept:
        m_numChannels (other.m_numChannels),
        m_startTimestampSeconds (other.m_startTimestampSeconds),
        dspChain (std::move (other.dspChain))
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
        dspChain.clear();

        if (other.dspChain.size() == 0)
            return *this;

        for (auto& dsp : other.dspChain)
            dspChain.push_back (dsp->copy());
        
        return *this;
    }

    /// @brief Moves from other signal
    SignalBase& operator= (SignalBase&& other) noexcept
    {
        if (this == &other)
            return *this;

        m_numChannels = other.m_numChannels;
        dspChain = std::move (other.dspChain);
        m_startTimestampSeconds = other.m_startTimestampSeconds;
        other.m_numChannels = 0;
        other.m_startTimestampSeconds = 0.0;

        return *this;
    }

    /// @brief Tells the host whether this Signal is capable of generating audio for a certain amount of cchannels
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
    virtual void renderNextBlock (AudioBuffer<SampleType>& output) = 0;

    /// @brief Resets the Signal to initial state
    /// @details Ideally should be implemented in a way that audio produced after resetting is identical to audio produced after instantiation
    virtual void reset() = 0;

    /// @brief Returns a smart pointer with a copy of this object
    /// @details Just put one of those two macros into your class body, and your @ref copy() and @ref move() are sorted:
    ///  - @ref HART_SIGNAL_DEFINE_COPY_AND_MOVE() for movable and copyable classes
    ///  - @ref HART_SIGNAL_FORBID_COPY_AND_MOVE for non-movable and non-copyable classes
    ///
    /// Read their description, and choose one that fits your class.
    /// You can, of course, make your own implementation, but you're not supposed to, unless you're doing something obscure.
    virtual std::unique_ptr<SignalBase<SampleType>> copy() const = 0;

    /// @brief Returns a smart pointer with a moved instance of this object
    /// @details Just pick a macro to define it - see description for @ref copy() for details
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

        // TODO: Check if all the effects in the chain support those settings first

        for (auto& dsp : dspChain)
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

        for (auto& dsp : dspChain)
            dsp->processWithEnvelopes (inputReplacing, output);
    }

    /// @brief Resets to Signal and all the effects attached to its DSP chain to initial state
    /// @details This method is intended to be called by hosts like AudioTestBuilder or Matcher.
    /// If you're not making a custom host, you probably don't need this method.
    virtual void resetWithDSPChain()
    {
        reset();

        for (auto& dsp : dspChain)
            dsp->reset();
    }

    /// @brief Makes a text representation of this signal and its entire signal chain for test failure outputs.
    /// @details Used by "<<" operator
    /// @private
    /// @param[out] stream Output stream to write to
    void representWithDSPChain (std::ostream& stream) const
    {
        represent (stream);

        for (const auto& dsp : dspChain)
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
    std::vector<std::unique_ptr<DSPBase<SampleType>>> dspChain;

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
/// @tparam Derived Subclass for CRTP
template<typename SampleType, typename Derived>
class Signal:
    public SignalBase<SampleType>
{
public:
    /// @brief Adds a DSP effect to the end of signal's DSP chain by copying it
    /// @note For DSP object that do not support copying or moving, use version of this method that takes a ```unique_ptr``` instead
    /// @param dsp A DSP effect instance
    Derived& followedBy (const DSPBase<SampleType>& dsp)
    {
        this->dspChain.emplace_back (dsp.copy());
        return static_cast<Derived&> (*this);
    }

    /// @brief Adds a DSP effect to the end of signal's DSP chain by transfering a smart pointer
    /// @note For DSP object that do not support copying or moving, use version of this method that takes a ```unique_ptr``` instead
    /// @param dsp A DSP effect instance
    Signal& followedBy (std::unique_ptr<DSPBase<SampleType>> dsp)
    {
        this->dspChain.emplace_back (std::move (dsp));
        return static_cast<Derived&> (*this);
    }

    // TODO: Add check if rvalue
    /// @brief Adds a DSP effect to the end of signal's DSP chain by moving it
    /// @note For DSP object that do not support copying or moving, use version of this method that takes a ```unique_ptr``` instead
    /// @param dsp A DSP effect instance
    template <
        typename DerivedDSP,
        typename = typename std::enable_if<
            std::is_base_of<
                DSPBase<SampleType>,
                typename std::decay<DerivedDSP>::type
                >::value
            >::type
        >
    Signal& followedBy (DerivedDSP&& dsp)
    {
        this->dspChain.emplace_back (dsp.move());
        return static_cast<Derived&> (*this);
    }

    std::unique_ptr<SignalBase<SampleType>> copy() const override
    {
        return hart::make_unique<Derived> (static_cast<const Derived&> (*this));
    }

    std::unique_ptr<SignalBase<SampleType>> move() override
    {
        return hart::make_unique<Derived> (std::move (static_cast<const Derived&> (*this)));
    }

    /// @brief Skips the signal to a specific timestamp
    /// @details Fast-forwards the signal, with all attaches DSP effects and their automation
    /// envelopes. Calling it multiple times on one instance will stack the skip times.
    /// @note Keep in mind that the skip is accurate within one audio frame tolerance
    /// @param startTimestampSeconds How much time to skip from the start of the signal
    Derived& skipTo (double startTimestampSeconds)
    {
        if (startTimestampSeconds < 0)
            HART_THROW_OR_RETURN (hart::ValueError, "Can't skip to a negative timestamp", static_cast<Derived&> (*this));

        this->m_startTimestampSeconds += startTimestampSeconds;
        return static_cast<Derived&> (*this);
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

/// @brief Adds a DSP effect to the end of signal's DSP chain by moving it
/// @relates Signal
/// @ingroup Signals
template<
    typename SampleType,
    typename DerivedSignal,
    typename DerivedDSP, typename std::enable_if<std::is_base_of<DSPBase<SampleType>, typename std::decay<DerivedDSP>::type>::value>::type>
Signal<SampleType, DerivedSignal>& operator>> (Signal<SampleType, DerivedSignal>& signal, DerivedDSP&& dsp)
{
    return signal.followedBy (std::move (dsp));
}

/// @brief Adds a DSP effect to the end of signal's DSP chain by copying it
/// @relates Signal
/// @ingroup Signals
template<typename SampleType, typename DerivedSignal>
Signal<SampleType, DerivedSignal>& operator>> (Signal<SampleType, DerivedSignal>& signal, const DSPBase<SampleType>& dsp)
{
    return signal.followedBy (dsp);
}

/// @brief Adds a DSP effect to the end of signal's DSP chain by copying it
/// @relates Signal
/// @ingroup Signals
template<typename SampleType, typename DerivedSignal>
Signal<SampleType, DerivedSignal>&& operator>> (Signal<SampleType, DerivedSignal>&& signal, const DSPBase<SampleType>& dsp)
{
    return std::move (signal.followedBy (dsp));
}

/// @brief Adds a DSP effect to the end of signal's DSP chain by transfering it
/// @details This is for smart pointers to abstract DSP type
/// @relates Signal
/// @ingroup Signals
template<typename SampleType, typename DerivedSignal>
Signal<SampleType, DerivedSignal>& operator>> (Signal<SampleType, DerivedSignal>& signal, std::unique_ptr<DSPBase<SampleType>>&& dsp)
{
    signal.followedBy (std::move (dsp));
    return signal;
}

/// @brief Adds a DSP effect to the end of signal's DSP chain by transfering it
/// @details This is for smart pointers to abstract DSP type
/// @relates Signal
/// @ingroup Signals
template<typename SampleType, typename DerivedSignal>
Signal<SampleType, DerivedSignal>&& operator>> (Signal<SampleType, DerivedSignal>&& signal, std::unique_ptr<DSPBase<SampleType>>&& dsp)
{
    signal.followedBy (std::move (dsp));
    return std::move (signal);
}

/// @brief Adds a DSP effect to the end of signal's DSP chain by transfering it
/// @details This is for smart pointers to actual (derived) DSP type
/// @relates Signal
/// @ingroup Signals
template<
    typename SampleType,
    typename DerivedSignal,
    typename DerivedDSP, typename = typename std::enable_if<std::is_base_of<DSPBase<SampleType>, DerivedDSP>::value>::type>
Signal<SampleType, DerivedSignal>& operator>>(Signal<SampleType, DerivedSignal>& signal, std::unique_ptr<DerivedDSP>&& dsp)
{
    signal.followedBy (std::move (dsp));
    return signal;
}

/// @brief Adds a DSP effect to the end of signal's DSP chain by transfering it
/// @details This is for smart pointers to actual (derived) DSP type
/// @relates Signal
/// @ingroup Signals
template<
    typename SampleType,
    typename DerivedSignal,
    typename DerivedDSP, typename = typename std::enable_if<std::is_base_of<DSPBase<SampleType>, DerivedDSP>::value>::type>
Signal<SampleType, DerivedSignal>&& operator>>(Signal<SampleType, DerivedSignal>&& signal, std::unique_ptr<DerivedDSP>&& dsp)
{
    signal.followedBy (std::move (dsp));
    return std::move (signal);
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
