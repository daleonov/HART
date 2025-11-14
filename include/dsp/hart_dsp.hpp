#pragma once

#include <algorithm>  // fill()
#include <memory>
#include <ostream>
#include <string>
#include <unordered_map>

#include "hart_audio_buffer.hpp"
#include "envelopes/hart_envelope.hpp"

namespace hart
{
template <typename SampleType> class AudioTestBuilder;
template <typename SampleType> class Signal;

/// @brief Hash table of automation envelope sequences mapped to param ids.
/// @details Keys: Param IDs (int enums like GainDb::gainDb)
///          Values: Sequence of automation envelope values for this Param ID, one value per frame
using EnvelopeBuffers = std::unordered_map<int, std::vector<double>>;

/// @defgroup DSP
/// @brief Used to process signals

/// @brief Base for all DSP effect objects
/// @details This class is used both for adapting your DSP classes that you wish to test,
/// and for using in DSP chains of @ref Signals, so you can use stock effects like @ref GainDb
/// as tested processors, and you can use your tested DSP subclasses in Signals' DSP chains with
/// other effects. You can even chain multiple of your own DSP classes together this way.
/// All the callbacks of this class are guaranteed to be called from the same thread.
/// @tparam SampleType Type of values that will be processed, typecally ```float``` or ```double```
/// @ingroup DSP
template <typename SampleType>
class DSP
{
public:

    /// @brief Prepare for processing
    /// @details This method is guaranteed to be called after @ref supportsChannelLayout, but before @ref process().
    /// In real time DSP, such methods are usually used for allocating memory and other non-realtime-safe and heavyweight
    /// operations, but remember that HART does not do real-time processing, so this merely to follow a common real-time DSP
    /// design conventions, where non-realtime operations are done in a separate callback like this one.
    /// @param maxBlockSizeFrames Maximum block size in frames (samples)
    virtual void prepare (double sampleRateHz, size_t numInputChannels, size_t numOutputChannels, size_t maxBlockSizeFrames) = 0;

    /// @brief Processes the audio
    /// @details Depending on circumstances, this callback will either be called to process an entire piece of audio all at once,
    /// or called repeatedly, one block at a time (see @ref AudioTestBuilder::withBlockSize()). In latter case, remember that the very
    /// last block of audio is almost always smaller than the block size set in @ref prepare(), so be careful with buffer bounds.
    /// All audio blocks except the last one are guaranteed to be equal to ```maxBlockSizeFrames``` set in @ref prepare() callback.
    /// It is guaranteed that input and output buffers are equal in length in frames (samples) to each,
    /// but they might have different number of channels. Use @ref supportsChannelLayout() to indicate
    /// whether the effect supports a specific i/o configuration or not, as it will be called before @ref prepare().
    /// @warning This method may be called in replacing manner, i. e. ```input``` and ```output``` may be references to the same object.
    /// @note Note that this method does not have to be real-time safe, as all rendering allways happens offline.
    /// Also note that, unlike real-time audio applications, this method is called on the same thread as all others like @ref prepare().
    /// @param input Input audio block
    /// @param output Output audio block
    virtual void process (const AudioBuffer<SampleType>& input, AudioBuffer<SampleType>& output, const EnvelopeBuffers& envelopeBuffers) = 0;

    /// @brief Resets to initial state
    /// @details Ideally should be implemented in a way that audio produced after resetting is identical to audio produced after instantiation
    virtual void reset() = 0;

    /// @brief Sets DSP value
    /// @param paramId Some ID that your subclass understands;
    /// use of enums is encouraged for readability
    /// @param value Value of the param in an appropriate unit;
    /// use of SI units is enocuraged (i.e. s instead of ms. Hz instead of kHz) to make better use of unit literals (see @ref Units)
    /// @warning This method is only called to set initial value before processing, and is not called to do automation (via @ref Envelopes)
    /// If you want your class to support automation for a specific parameter, override @ref supportsEnvelopeFor(), then call @ref hasEnvelopeFor()
    /// in your @ref process() to check is there's an automation curve for a apecific parameter, and then call @ref getValues() to retreive
    /// the automation envelope values.
    virtual void setValue (int paramId, double value) = 0;

    /// @brief Retreives DSP value
    /// @details Among other things, it can be used to retreive various readings like Gain Reduction measurements from your effect for further inspection
    /// @param paramId Some ID that your subclass understands
    /// @return The value of requested parameter in a unit that your subclass understands
    /// @note This method is only intended for parameters that don't have an automation envelope attached to this specific instance.
    /// To get values for automated parameters, use @ref getValues() instead.
    virtual double getValue (int paramId) const = 0;

    /// @brief Tells the runner (host) whether this effect supports a specific i/o configuration.
    /// @details It is guaranteed that the effect will not receive unsupported number of channels in @ref process().
    /// However, it is not always to handle gracefully channel layout being unsupported, so in some circumstances
    /// it can cause an exception or a test failure. This method is guaranteed to be called at least once before @ref prepare()
    virtual bool supportsChannelLayout (size_t numInputChannels, size_t numOutputChannels) const = 0;

    /// @brief Makes a text representation of this object for test failure outputs.
    /// @brief It is strongly encouraged to follow python's
    /// <a href="https://docs.python.org/3/reference/datamodel.html#object.__repr__" target="_blank">repr()</a>
    /// conventions for returned text - basically, return something like "MyClass(value1, value2)" whenever possible,
    /// or "<Readable info in angled brackets>" otherwise.
    /// @param[out] stream Output stream to write to
    virtual void print (std::ostream& stream) const = 0;

    /// @brief Tells whether this effect accepts automation envelopes for a particular parameter
    /// @param paramId Some ID that your subclass understands
    /// @return true if your subclass can process automation for this parameter, false otherwise
    virtual bool supportsEnvelopeFor (int paramId) const { return false; }

    /// @brief Return a smart pointer with a copy of this object
    /// @details Use @ref HART_DSP_DECLARE_COPY_METHOD() to define this method if your class is trivially copyable
    virtual std::unique_ptr<DSP<SampleType>> copy() const = 0;

    /// @brief Destructor
    virtual ~DSP() = default;

    /// @brief Default constructor
    DSP() = default;

    /// @brief Copies from another DSP effect instance
    /// @details Attached automation envelopes are deep-copied
    DSP (const DSP& other)
    {
        for (auto& pair : other.m_envelopes)
            m_envelopes.emplace (pair.first, pair.second->copy());
    }

    /// @brief Move constructor
    /// @details Attached automation envelopes are moved as well
    DSP (DSP&& other) noexcept
        : m_envelopes (std::move (other.m_envelopes))
    {
    }

    /// @brief Copies from another DSP effect instance
    /// @details Attached automation envelopes are deep-copied
    DSP& operator= (const DSP& other)
    {
        if (this == &other)
            return *this;

        for (auto& pair : other.m_envelopes)
            m_envelopes.emplace (pair.first, pair.second->copy());

        return *this;
    }

    /// @brief Move assignment
    /// @details Attached automation envelopes are moved as well
    DSP& operator= (DSP&& other) noexcept
    {
        if (this != &other)
            m_envelopes = std::move (other.m_envelopes);

        return *this;
    }

    /// @brief Adds envelope to a specific parameter by moving it
    /// @details Guaranteed to be called strictly after the @ref supportsEnvelopeFor() callback,
    /// and only if it has returned ```true``` for this specific ```paramId```.
    /// Can be chained together like ```myEffect.withEnvelope (someId, someEnvelope).withEnvelope (otherId, otherEnvelope)```.
    /// If called multiple times for the same paramId, only last envelope for this ID will be used, all previous ones will be descarded.
    DSP& withEnvelope (int paramId, Envelope&& envelope)
    {
        if (! supportsEnvelopeFor(paramId))
            HART_THROW_OR_RETURN (hart::UnsupportedError, std::string ("DSP doesn't support envelopes for param ID: ") + std::to_string (paramId), *this);

        m_envelopes.emplace (paramId, std::make_unique<Envelope> (std::move (envelope)));
        return *this;
    }

    /// @brief Adds envelope to a specific parameter by copying it
    DSP& withEnvelope (int paramId, const Envelope& envelope)
    {
        if (! supportsEnvelopeFor(paramId))
            HART_THROW_OR_RETURN (hart::UnsupportedError, std::string ("DSP doesn't support envelopes for param ID: ") + std::to_string (paramId), *this);

        m_envelopes.emplace (paramId, envelope.copy());
        return *this;
    }

    /// @brief Checks if there's an automation envelope attached to a specific parameter
    /// @details You may use it in @ref prepare() or @ref process() to check if there's
    /// an automation envelope is attached to a specific instance of your effect.
    /// The envelopes are guaranteed to be attached strictly before @ref prepare() callback,
    /// so by the time of the first @ref process() call consider the presence or absence
    /// of envelope permanent.
    bool hasEnvelopeFor (int paramId)
    {
        return m_envelopes.find (paramId) != m_envelopes.end();
    }

protected:
    std::unordered_map<int, std::unique_ptr<Envelope>> m_envelopes;

    // TODO: User should probably just get a container with pre-rendered values, and never call this method themselves - too much to worry about otherwise
    /// @brief Gets sample-accurate automation envelope values for a specific parameter
    /// @details You're supposed to call it in the @ref process() to render the values for the current block
    /// @warning Make sure to only call it when the envelope for a specific parameter indeed exists! See @ref hasEnvelopeFor()
    /// @param[in] paramId Some ID that your subclass understands
    /// @param[in] blockSize Buffer size in frames, should be the same as ```input```/```output```'s size in @ref process()
    /// @param[out] valuesOutput Container to get filled with the rendered automation values
    void getValues (int paramId, size_t blockSize, std::vector<double>& valuesOutput)
    {
        if (valuesOutput.size() < blockSize)
        {
            HART_WARNING ("Make sure to configure your envelope container size before processing audio");
            valuesOutput.resize (blockSize);
        }

        if (! hasEnvelopeFor (paramId))
        {
            const double value = getValue (paramId);
            std::fill (valuesOutput.begin(), valuesOutput.end(), value);
        }
        else
        {
            m_envelopes[paramId]->renderNextBlock (blockSize, valuesOutput);
        }
    }

    /// @brief Gets sample-accurate automation envelope values for a specific parameter
    /// @details Same as its namesake, but lazy-allocates the memory instead of already having one on hand
    std::vector<double> getValues (int paramId, size_t blockSize)
    {
        std::vector<double> values (blockSize);
        getValues (paramId, values);
        return values;
    }

protected:
    EnvelopeBuffers m_envelopeBuffers;
    friend class AudioTestBuilder<SampleType>;
    friend class Signal<SampleType>;

    void prepareWithEnvelopes (double sampleRateHz, size_t numInputChannels, size_t numOutputChannels, size_t maxBlockSizeFrames)
    {
        m_envelopeBuffers.clear();  // TODO: Remove only unused buffers

        for (auto& item : m_envelopes)
        {
            const int paramId = item.first;
            m_envelopeBuffers.emplace (paramId, std::vector<double> (maxBlockSizeFrames));
        }

        hassert (m_envelopes.size() == m_envelopeBuffers.size());

        for (auto& item : m_envelopeBuffers)
        {
            const int paramId = item.first;
            auto& envelopeBuffer = item.second;

            // Sanity checks
            hassert (supportsEnvelopeFor (paramId) && "Envelope for this id is unsupported, yet there's an envelope buffer allocated for it");
            hassert (hasEnvelopeFor (paramId) && "Envelope for this param id is not attached, yet there's an envelope buffer allocated for it");

            if (envelopeBuffer.size() != maxBlockSizeFrames)
                envelopeBuffer.resize (maxBlockSizeFrames);
        }

        prepare (sampleRateHz, numInputChannels, numOutputChannels, maxBlockSizeFrames);
    }

    void processWithEnvelopes (const AudioBuffer<SampleType>& input, AudioBuffer<SampleType>& output)
    {
        for (auto& item : m_envelopeBuffers)
        {
            const int paramId = item.first;
            auto& envelopeBuffer = item.second;

            // Sanity checks
            hassert (supportsEnvelopeFor (paramId) && "Envelope for this id is unsupported, yet there's an envelope buffer allocated for it");
            hassert (hasEnvelopeFor (paramId) && "Envelope for this param id is not attached, yet there's an envelope buffer allocated for it");
            hassert (input.getNumFrames() <= envelopeBuffer.size() && "Envelope Buffers were not allocated properly for this buffer size");

            // Render envelope values
            getValues (paramId, envelopeBuffer.size(), envelopeBuffer);
        }
        
        process (input, output, m_envelopeBuffers);
    }
};

/// @brief Prints readable text representation of the DSP object into the I/O stream
template <typename SampleType>
inline std::ostream& operator<< (std::ostream& stream, const DSP<SampleType>& dsp) {
    dsp.print (stream);
    return stream;
}

/// @brief A handy macro for trivially copyable DSP objects
/// @details Despite returning a smart pointer to an abstract DSP class,
/// it must construct an object of a specific class, hence the boilerplate method
#define HART_DSP_DECLARE_COPY_METHOD(cls) \
    std::unique_ptr<DSP<SampleType>> copy() const override \
    { \
        return std::make_unique<cls> (*this); \
    }

}  // namespace hart
