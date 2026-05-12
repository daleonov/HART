#pragma once

#include <memory>
#include <string>

#include "hart_analysis_context.hpp"
#include "hart_audio_buffer.hpp"
#include "hart_channel_flags.hpp"
#include "hart_exceptions.hpp"
#include "hart_matcher_failure_details.hpp"
#include "hart_utils.hpp"  // make_unique(), HART_DEPRECATED()

/// @defgroup Matchers Matchers
/// @brief Check audio

namespace hart
{

/// @brief Polymorphic base for all matchers
/// @warning This class exists only for type erasure and polymorphism.
/// Do NOT inherit custom matchers from this class directly.
/// Inherit from @ref hart::Matcher instead.
/// @ingroup Matchers
template<typename SampleType>
class MatcherBase
{
public:
    virtual ~MatcherBase() = default;

    /// @brief Prepare for processing
    /// It is guaranteed that all subsequent process() calls will be in line with the arguments received in this callback.
    /// This callback is guaranteed to be called after canOperatePerBlock()
    /// If any of the values supplied by this callback are not supported by the matcher, it is expected to act as if
    /// the match has failed when match() gets called.
    /// @param sampleRateHz sample rate at which the audio should be interpreted
    /// @param numInputChannels Number of channels in the input (reference) audio
    /// @param numOutputChannels Number of channels in the output (observed) audio
    /// @param maxBlockSizeFrames Maximum block size in frames (samples)
    virtual void prepare (double sampleRateHz, size_t numInputChannels, size_t numOutputChannels, size_t maxBlockSizeFrames) = 0;

    /// @brief Tells the host if the piece of audio satisfies Matcher's condition or not
    /// @details It is guaranteed to be called only after `prepare()`, or not be called at all.
    /// It is guaranteed to be handed a pair of `AudioBuffer`s in line with values set by the last `prepare()` call.
    /// If `canOperatePerBlock()` has returned `false`, this callback is guaranteed to be handed a full piece of
    /// audio to check. Otherwise, it may still get a full piece of audio, or get data on a block-by-block basis.
    /// @param inputAudio A piece of input audio. Some matchers may be based on a relationship between input and output,
    /// rather than checking just the output audio. And in a lot of cases Matcher can ingore the input completely.
    /// @param observedOutputAudio A piece of observed output audio to check
    /// @returns `true` if the audio satisfies the Matcher's condition, `false` otherwise
    /// @deprecated This method is deprecated. Switch to the one that uses AnalysisContext instead.
    HART_DEPRECATED ("This method is deprecated. Switch to the one that uses AnalysisContext instead.")
    virtual bool match (const AudioBuffer<SampleType>& inputAudio, const AudioBuffer<SampleType>& observedOutputAudio)
    {
        const AnalysisContext<SampleType> analysisContext (inputAudio, observedOutputAudio);
        return match (analysisContext); 
    }

    /// @brief Tells the host if the piece of audio satisfies Matcher's condition or not
    /// @details It is guaranteed to be called only after `prepare()`, or not be called at all.
    /// It is guaranteed to be handed a pair of `AudioBuffer`s in line with values set by the last `prepare()` call.
    /// If `canOperatePerBlock()` has returned `false`, this callback is guaranteed to be handed a full piece of
    /// audio to check. Otherwise, it may still get a full piece of audio, or get data on a block-by-block basis.
    /// @param analysisContext Carries everything matcher should know about the input and output audio.
    /// @returns `true` if the audio satisfies the Matcher's condition, `false` otherwise
    virtual bool match (AnalysisContext<SampleType> analysisContext)
    {
        return match (analysisContext.inputAudio(), analysisContext.outputAudio());
    }

    /// @brief Tells the host if it can operate on a block-by-block basis
    /// @details Some types of conditions absolutely require having a full piece of audio
    /// to produce an appropriate response. For example, @ref hart::PeaksAt matcher.
    /// Those types of matchers will return false on this callback.
    /// Matcher is guaranteed to receive a full piece of audio if this callback has
    /// returned \c false. Otherwise, it may receive audio either block-by-block
    /// basis, or still get a full piece of audio, if the host decides to do so.
    virtual bool canOperatePerBlock() const = 0;

    /// @brief Resets the matcher to its initial state
    virtual void reset() {}

    /// @brief Returns a description of why the match has failed
    /// @details It is guaranteed to be called strictly after calling match(), or not called at all
    /// @note This method is a callback for the test host, so you probably don't need to call it
    /// yourself ever. If you're making a custom matcher, use it to communicate the data with test host.
    /// @retval MatcherFailureDetails::frame Index of frame at which the match has failed
    /// @retval MatcherFailureDetails::channel Index of channel at which the failure was detected
    /// @retval MatcherFailureDetails::description Readable description of why the match has failed.
    /// Do not include the value of observed frame value or its timing in the description, as well as
    /// any of values printed by represent(), as all of this will be added to the output anyway.
    /// Also, query @ref hart::CLIConfig for number of displayed decimal places, whenever applicable.
    /// @see MatcherFailureDetails
    virtual MatcherFailureDetails getFailureDetails() const = 0;

    /// @brief Makes a text representation of this Matcher for test failure outputs.
    /// @details It is strongly encouraged to follow python's
    /// <a href="https://docs.python.org/3/reference/datamodel.html#object.__repr__" target="_blank">repr()</a>
    /// conventions for returned text - basically, put something like "MyClass(value1, value2)" (with no quotes)
    /// into the stream whenever possible, or "<Readable info in angled brackets>" otherwise.
    /// Also, use built-in stream manipulators like @ref hart::dbPrecision wherever applicable.
    /// Use @ref HART_DEFINE_GENERIC_REPRESENT() to get a basic implementation for this method.
    /// @param[out] stream Output stream to write to
    virtual void represent (std::ostream& stream) const = 0;

    /// @brief Tells the host whether this Matcher is capable of operating on audio with a specific number of channels
    /// @details It is guaranteed that the matcher will not receive unsupported number of channels in @ref match().
    /// This method is guaranteed to be called at least once before @ref prepare()
    /// @param numInputChannels Number of channels in input (reference) buffer that will need to be processed
    /// @param numOutputChannels Number of channels in output (observed) buffer that will need to be processed
    /// @return true if signal is capable of processing audio with requested number of channels, false otherwise
    virtual bool supportsChannelLayout (size_t /* numInputChannels */, size_t /* numOutputChannels */) const { return true; }

    /// @brief Tells whether this Matcher supports given sample rate
    /// @details It is guaranteed to be called before @ref prepare()
    /// @param sampleRateHz sample rate at which the audio will be presented
    /// @return true if matcher is capable of processing audio with a given sample rate, false otherwise
    virtual bool supportsSampleRate (double /* sampleRateHz */) const { return true; }

    /// @brief Returns a smart pointer with a copy of this object
    /// @return Copy of this object wrapped in a smart pointer
    virtual std::unique_ptr<MatcherBase<SampleType>> copy() const = 0;

    /// @brief Returns a smart pointer with a moved instance of this object
    /// @return This object, moved and wrapped in a smart pointer
    virtual std::unique_ptr<MatcherBase<SampleType>> move() = 0;

    /// @brief Makes a text representation of this Matcher with optional "atChannels" appendix
    /// @details For internal use by hosts, don't override it in custom matchers.
    virtual void representWithActiveChannels (std::ostream& stream) const = 0;
};

/// @brief Base for audio matchers
/// @details Those matchers get a piece of audio generated by some DSP effect and asked if if satisfies their condition or not
/// You can inherit from this class to create a new matcher
/// (note the <a href="https://en.cppreference.com/w/cpp/language/crtp.html">CRTP</a>
/// in the template args here, it's important!):
/// @code{.cpp}
/// template<typename SampleType>
/// class MyCustomMatcher: public Matcher<SampleType, MyCustomMatcher<SampleType>>
/// {
///     // ...
/// };
/// @endcode
/// @tparam SampleType Type of data in the buffers to be checked, typically `float` or `double`.
/// @tparam Derived Subclass for CRTP
/// @ingroup Matchers
template<typename SampleType, typename Derived>
class Matcher:
    public MatcherBase<SampleType>
{

public:
    /// @brief Returns a smart pointer with a copy of this object
    /// @return Copy of this object wrapped in a smart pointer
    virtual std::unique_ptr<MatcherBase<SampleType>> copy() const override
    {
        return hart::make_unique<Derived> (static_cast<const Derived&> (*this));
    }

    /// @brief Returns a smart pointer with a moved instance of this object
    /// @return This object, moved and wrapped in a smart pointer
    virtual std::unique_ptr<MatcherBase<SampleType>> move() override
    {
        return hart::make_unique<Derived> (std::move (static_cast<Derived&> (*this)));
    }

    /// @brief Makes this matcher check only specific channels, and ignore the rest
    /// @details If not set, the matcher applies to all channels by default.
    /// If you call this method multiple times, only the last one will be applied.
    /// To select only one channel, consider using @ref Matcher::atChannel() instead.
    /// @param channelsToMatch List of channels this matcher should apply to,
    /// e.g. `{0, 1}` or `{Channel::left, Channel::right}` for left and right channels only.
    /// @see `hart::Channel`
    Derived& atChannels (std::initializer_list<size_t> channelsToMatch) &
    {
        _atChannels (channelsToMatch);
        return static_cast<Derived&> (*this);
    }

    /// @brief Makes this matcher check only specific channels, and ignore the rest
    /// @details If not set, the matcher applies to all channels by default.
    /// If you call this method multiple times, only the last one will be applied.
    /// To select only one channel, consider using @ref Matcher::atChannel() instead.
    /// @param channelsToMatch List of channels this matcher should apply to,
    /// e.g. `{0, 1}` or `{Channel::left, Channel::right}` for left and right channels only.
    /// @see `hart::Channel`
    Derived&& atChannels (std::initializer_list<size_t> channelsToMatch) &&
    {
        _atChannels (channelsToMatch);
        return static_cast<Derived&&> (*this);
    }

    /// @brief Makes this matcher check only one specific channel, and ignore the rest
    /// @details If not set, the matcher applies to all channels by default.
    /// If you call this method multiple times, only the last one will be applied.
    /// To select multiple channels, use @ref Matcher::atChannels() instead.
    /// @param channelToMatch Channel this matcher should apply to (zero-based),
    /// e.g. `0` or `Channel::left` for left channel.
    /// @note If not set, the matcher applies to all channels by default
    /// @see `hart::Channel`
    Derived& atChannel (size_t channelToMatch) &
    {
        _atChannel (channelToMatch);
        return static_cast<Derived&> (*this);
    }

    /// @brief Makes this matcher check only one specific channel, and ignore the rest
    /// @details If not set, the matcher applies to all channels by default.
    /// If you call this method multiple times, only the last one will be applied.
    /// To select multiple channels, use @ref Matcher::atChannels() instead.
    /// @param channelToMatch Channel this matcher should apply to (zero-based),
    /// e.g. `0` or `Channel::left` for left channel.
    /// @note If not set, the matcher applies to all channels by default
    /// @see `hart::Channel`
    Derived&& atChannel (size_t channelToMatch) &&
    {
        _atChannel (channelToMatch);
        return static_cast<Derived&&> (*this);
    }

    /// @brief Makes this matcher check all channels
    /// @details This is the default setting anyway, so this method is only
    /// for cases when you need to override previous `atChannel()`,
    /// `atChannels()` or `atAllChannelsExcept()` calls.
    Derived& atAllChannels() &
    {
        _atAllChannels();
        return static_cast<Derived&> (*this);
    }

    /// @brief Makes this matcher check all channels
    /// @details This is the default setting anyway, so this method is only
    /// for cases when you need to override previous `atChannel()`,
    /// `atChannels()` or `atAllChannelsExcept()` calls.
    Derived&& atAllChannels() &&
    {
        _atAllChannels();
        return static_cast<Derived&&> (*this);
    }

    /// @brief Makes this matcher check only specific channels, and ignore the rest
    /// @details If not set, matcher checks all channels by default.
    /// If you call this method multiple times, only the last one will be applied.
    /// @param channelsToSkip List of channels this matcher should NOT check,
    /// e.g. `{0, 1}` or `{Channel::left, Channel::right}` to skip left and right
    /// channels, and match the rest
    /// @see `hart::Channel`
    Derived& atAllChannelsExcept (std::initializer_list<size_t> channelsToSkip) &
    {
        _atAllChannelsExcept (channelsToSkip);
        return static_cast<Derived&> (*this);
    }

    /// @brief Makes this matcher check only specific channels, and ignore the rest
    /// @details If not set, matcher checks all channels by default.
    /// If you call this method multiple times, only the last one will be applied.
    /// @param channelsToSkip List of channels this matcher should NOT check,
    /// e.g. `{0, 1}` or `{Channel::left, Channel::right}` to skip left and right
    /// channels, and match the rest
    /// @see `hart::Channel`
    Derived&& atAllChannelsExcept (std::initializer_list<size_t> channelsToSkip) &&
    {
        _atAllChannelsExcept (channelsToSkip);
        return static_cast<Derived&&> (*this);
    }

    // TODO: Move to MatcherBase together with m_channelsToMatch to make it non-virtual?
    void representWithActiveChannels (std::ostream& stream) const override
    {
        this->represent (stream);

        if (m_channelsToMatch.allTrue())
            return;

        stream << ".atChannels (";
        m_channelsToMatch.representAsInitializerList (stream);
        stream << ')';
    }

protected:
    // TODO: Resize m_channelsToMatch() in host's version of prepare() method
    ChannelFlags m_channelsToMatch {true};

    /// @brief Indicates whether this matcher should check a specific channel
    /// @details You might want to use it in the @ref match() callback.
    /// See @ref atChannels(), @ref atChannel(), @ref atAllChannels(), @ref atAllChannelsExcept().
    bool appliesToChannel (size_t channel)
    {
        return m_channelsToMatch[channel];
    }

private:
    void _atChannels (std::initializer_list<size_t> channelsToMatch)
    {
        m_channelsToMatch.setAllTo (false);

        for (size_t channel : channelsToMatch)
        {
            if (channel >= m_channelsToMatch.size())
                HART_THROW_OR_RETURN_VOID (hart::ValueError, "Channel exceeds max number of channels");

            m_channelsToMatch[channel] = true;
        }
    }

    void _atChannel (size_t channelToMatch)
    {
        if (channelToMatch >= m_channelsToMatch.size())
            HART_THROW_OR_RETURN_VOID (hart::ValueError, "Channel exceeds max number of channels");

        m_channelsToMatch.setAllTo (false);
        m_channelsToMatch[channelToMatch] = true;
    }

    void _atAllChannels()
    {
        m_channelsToMatch.setAllTo (true);
    }

    void _atAllChannelsExcept (std::initializer_list<size_t> channelsToSkip)
    {
        m_channelsToMatch.setAllTo (true);

        for (size_t channel : channelsToSkip)
        {
            if (channel >= m_channelsToMatch.size())
                HART_THROW_OR_RETURN_VOID (hart::ValueError, "Channel exceeds max number of channels");

            m_channelsToMatch[channel] = false;
        }
    }
};

/// @brief Prints readable text representation of the Matcher object into the I/O stream
/// @relates Matcher
/// @ingroup Matchers
template <typename SampleType>
inline std::ostream& operator<< (std::ostream& stream, const MatcherBase<SampleType>& matcher)
{
    matcher.representWithActiveChannels (stream);
    return stream;
}

}  // namespace hart

/// @private
#define HART_MATCHER_DECLARE_ALIASES_FOR(ClassName) \
    namespace aliases_float{\
        using ClassName = hart::ClassName<float>;\
    }\
    namespace aliases_double{\
        using ClassName = hart::ClassName<double>;\
    }
