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

/// @defgroup Signals Signals
/// @brief Generate signals

namespace hart {

/// @brief Base class for signals
/// @ingroup Signals
/// @tparam SampleType Type of values that will be generated, typically ```float``` or ```double```
template<typename SampleType>
class Signal
{
public:
    /// @brief Default constructor
    Signal() = default;

    /// @brief Copies other signal
    Signal (const Signal& other):
        m_numChannels(other.m_numChannels)
    {
        if (other.dspChain.size() == 0)
            return;

        dspChain.reserve (dspChain.size());

        for (auto& dsp : other.dspChain)
            dspChain.push_back (dsp->copy());
    }

    /// @brief Moves from other signal
    Signal (Signal&& other) noexcept:
        m_numChannels (other.m_numChannels),
        dspChain (std::move (other.dspChain))
    {
        other.m_numChannels = 0;
    }

    /// @brief Destructor
    virtual ~Signal() = default;

    /// @brief Copies from other signal
    Signal& operator= (const Signal& other)
    {
        if (this == &other)
            return *this;

        m_numChannels = other.m_numChannels;
        dspChain.reset();

        if (other.dspChain.size() == 0)
            return *this;

        for (auto& dsp : other.dspChain)
            dspChain.push_back (dsp->copy());
        
        return *this;
    }

    /// @brief Moves from other signal
    Signal& operator= (Signal&& other) noexcept
    {
        if (this == &other)
            return *this;

        m_numChannels = other.m_numChannels;
        dspChain = std::move (other.dspChain);
        other.m_numChannels = 0;

        return *this;
    }

    /// @brief Tells the host whether this Signal is capable of generating audio for a certain amount of cchannels
    /// @details It is guaranteed that the signal will not receive unsupported number of channels in @ref renderNextBlock().
    /// This method is guaranteed to be called at least once before @ref prepare()
    /// @note This method should only care about the Signal itself, and not the attached effects in DSP chain - they'll be queried separately
    /// @param numChannels Number of output channels that will need to be filled
    /// @return true if signal is capable of filling this many channels with audio, false otherwise
    virtual bool supportsNumChannels (size_t numChannels) const { return true; };

    /// @brief Tells whether this Signal supports given sample rate
    /// @details It is guaranteed to be called before @ref prepare()
    /// @note This method should only care about the Signal itself, and not the attached effects in DSP chain - they'll be queried separately
    /// @param sampleRateHz sample rate at which the audio should be generated
    /// @return true if signal is capable of generating audio at a given sample rate, false otherwise
    virtual bool supportsSampleRate (double sampleRateHz) const { return true; }

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
    virtual std::unique_ptr<Signal<SampleType>> copy() const = 0;

    /// @brief Returns a smart pointer with a moved instance of this object
    /// @details Just pick a macro to define it - see description for @ref copy() for details
    virtual std::unique_ptr<Signal<SampleType>> move() = 0;

    /// @brief Makes a text representation of this object for test failure outputs.
    /// @brief It is strongly encouraged to follow python's
    /// <a href="https://docs.python.org/3/reference/datamodel.html#object.__repr__" target="_blank">repr()</a>
    /// conventions for returned text - basically, put something like "MyClass(value1, value2)" (with no quotes)
    /// into the stream whenever possible, or "<Readable info in angled brackets>" otherwise.
    virtual std::string describe() const = 0;

    /// @brief Adds a DSP effect to the end of signal's DSP chain by copying it
    /// @note For DSP object that do not support copying or moving, use version of this method that takes a ```unique_ptr``` instead
    /// @param dsp A DSP effect instance
    Signal& followedBy (const DSP<SampleType>& dsp)
    {
        dspChain.emplace_back (dsp.copy());
        return *this;
    }

    // TODO: Add check if rvalue
    /// @brief Adds a DSP effect to the end of signal's DSP chain by moving it
    /// @note For DSP object that do not support copying or moving, use version of this method that takes a ```unique_ptr``` instead
    /// @param dsp A DSP effect instance
    template <typename DerivedDSP, typename = std::enable_if_t<std::is_base_of_v<DSP<SampleType>, std::decay_t<DerivedDSP>>>>
    Signal& followedBy (DerivedDSP&& dsp)
    {
        dspChain.emplace_back (
            std::make_unique<std::decay_t<DerivedDSP>> (std::forward<DerivedDSP> (dsp))
        );
        return *this;
    }

    // TODO: followedBy (std::unique_ptr<DSP<SampleType>> dsp)

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

    /// @brief Helper for template resolution
    /// @private
    using m_SampleType = SampleType;

private:
    size_t m_numChannels = 1;
    std::vector<std::unique_ptr<DSP<SampleType>>> dspChain;
};

// TODO: Overload >> for unique ptrs (multiple)

/// @brief Adds a DSP effect to the end of signal's DSP chain by moving it
/// @relates Signal
/// @ingroup Signals
template<typename SampleType, typename DerivedDSP, std::enable_if_t<std::is_base_of_v<DSP<SampleType>, std::decay_t<DerivedDSP>>>>
Signal<SampleType>& operator>> (Signal<SampleType>& signal, DerivedDSP&& dsp)
{
    return signal.followedBy (std::move (dsp));
}

/// @brief Adds a DSP effect to the end of signal's DSP chain by copying it
/// @relates Signal
/// @ingroup Signals
template<typename SampleType>
Signal<SampleType>& operator>> (Signal<SampleType>& signal, const DSP<SampleType>& dsp)
{
    return signal.followedBy (dsp);
}

/// @brief Adds a DSP effect to the end of signal's DSP chain by copying it
/// @relates Signal
/// @ingroup Signals
template<typename SampleType>
Signal<SampleType>&& operator>> (Signal<SampleType>&& signal, const DSP<SampleType>& dsp)
{
    return std::move (signal.followedBy (dsp));
}


}  // namespace hart

/// @brief Defines @ref hart::Signal::copy() and @ref hart::Signal::move() methods
/// @details Put this into your class body's ```public``` section if either is true:
///  - Your class is trivially copyable and movable
///  - You have your Rule Of Five methods explicitly defined in this class
/// (see <a href="https://en.cppreference.com/w/cpp/language/rule_of_three.html" target="_blank">Rule Of Three/Five/Zero</a>)
///
/// If neither of those is true, or you're unsure, use @ref HART_SIGNAL_FORBID_COPY_AND_MOVE instead
///
/// Despite returning a smart pointer to an abstract Signal class, those two methods must construct
/// an object of a specific class, hence the mandatory boilerplate methods - sorry!
/// @param ClassName Name of your class
/// @ingroup Signals
#define HART_SIGNAL_DEFINE_COPY_AND_MOVE(ClassName) \
    std::unique_ptr<Signal<SampleType>> copy() const override { \
        return std::make_unique<ClassName> (*this); \
    } \
    std::unique_ptr<Signal<SampleType>> move() override { \
        return std::make_unique<ClassName> (std::move (*this)); \
    }

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
///    .withInputSignal(std::make_unique<MyDspType>())  // As input signal
///    .expectTrue (EqualsTo (std::make_unique<MyDspType>()))  // As reference signal
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
