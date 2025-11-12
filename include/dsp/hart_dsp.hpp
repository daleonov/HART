#pragma once

#include <algorithm>  // fill()
#include <ostream>
#include <string>
#include <unordered_map>

#include "hart_audio_buffer.hpp"
#include "envelopes/hart_envelope.hpp"

namespace hart
{

template <typename SampleType>
class DSP
{
public:
    virtual ~DSP() = default;
    virtual void prepare (double sampleRateHz, size_t numInputChannels, size_t numOutputChannels, size_t maxBlockSizeFrames) = 0;
    virtual void process (const AudioBuffer<SampleType>& inputs, AudioBuffer<SampleType>& outputs) = 0;
    virtual void reset() = 0;
    virtual void setValue (int paramId, double value) = 0;
    virtual double getValue (int paramId) const = 0;
    virtual bool supportsChannelLayout (size_t numInputChannels, size_t numOutputChannels) const = 0;
    virtual void print (std::ostream& stream) const = 0;
    virtual bool supportsEnvelopeFor (int paramId) const = 0;

    DSP& withEnvelope (int paramId, Envelope&& envelope)
    {
        // TODO: Check supportsEnvelopeFor() first
        m_envelopes.emplace (paramId, std::make_unique<Envelope> (std::move (envelope)));
        return *this;
    }

    DSP& withEnvelope (int paramId, const Envelope& envelope)
    {
        // TODO: Check supportsEnvelopeFor() first
        m_envelopes.emplace (paramId, envelope.copy());
        return *this;
    }

    bool hasEnvelopeFor (int paramId)
    {
        return m_envelopes.find (paramId) != m_envelopes.end();
    }

protected:
    std::unordered_map<int, std::unique_ptr<Envelope>> m_envelopes;

    void getValues (int paramId, size_t blockSize, std::vector<double>& valuesOutput)
    {
        if (valuesOutput.size() != blockSize)
        {
            // TODO: Warning message
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

    std::vector<double> getValues (int paramId, size_t blockSize)
    {
        std::vector<double> values (blockSize);
        getValues (paramId, values);
        return values;
    }
};

template <typename SampleType>
inline std::ostream& operator<< (std::ostream& stream, const DSP<SampleType>& dsp) {
    dsp.print (stream);
    return stream;
}

}  // namespace hart
