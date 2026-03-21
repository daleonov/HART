#pragma once

#include <functional>

#include "hart_exceptions.hpp"
#include "matchers/hart_matcher.hpp"

// TODO: Add doxygen docs

namespace hart
{

/// @ingroup Matchers
template<typename SampleType>
class MatcherFunction:
    public Matcher<SampleType, MatcherFunction<SampleType>>
{
public:
    MatcherFunction (std::function <bool (const AudioBuffer<SampleType>&, const AudioBuffer<SampleType>&)> matcherFunction, const std::string& label = {}) :
        m_matcherFunctionForInputAndOutput (std::move (matcherFunction)),
        m_label (label)
    {
    }

    MatcherFunction (std::function <bool (const AudioBuffer<SampleType>&)> matcherFunction, const std::string& label = {}) :
        m_matcherFunctionForOutputOnly (std::move (matcherFunction)),
        m_label (label)
    {
    }

    bool match (const AudioBuffer<SampleType>& inputAudio, const AudioBuffer<SampleType>& observedOutputAudio) override
    {
        if (m_matcherFunctionForOutputOnly != nullptr)
            return m_matcherFunctionForOutputOnly (observedOutputAudio);

        if (m_matcherFunctionForInputAndOutput != nullptr)
            return m_matcherFunctionForInputAndOutput (inputAudio, observedOutputAudio);

        HART_THROW_OR_RETURN (hart::NullPointerError, "Matcher function is a nullptr!", false);
    }

    virtual MatcherFailureDetails getFailureDetails() const override
    {
        MatcherFailureDetails details;
        details.frame = 0;
        details.channel = 0;
        details.description = "Matcher function (" + (m_label.empty() ? "no label" : m_label) + ") has returned false";
        return details;
    }

    void represent (std::ostream& stream) const override
    {
        stream << "MatcherFunction (<function>, \"" << m_label << "\")";
    }

    bool canOperatePerBlock() override { return false;}
    void prepare (double /* sampleRateHz */, size_t /* numChannels */, size_t /* maxBlockSizeFrames */) override {}
    void reset() override {}

private:
    const std::function <bool (const AudioBuffer<SampleType>&, const AudioBuffer<SampleType>&)> m_matcherFunctionForInputAndOutput = nullptr;
    const std::function <bool (const AudioBuffer<SampleType>&)> m_matcherFunctionForOutputOnly = nullptr;
    const std::string m_label;
};

HART_MATCHER_DECLARE_ALIASES_FOR (MatcherFunction)

}  // namespace hart
