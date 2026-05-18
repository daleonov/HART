#pragma once

#include <cmath>  // abs()
#include <sstream>
#include <vector>

#include "hart_accurate_sum.hpp"
#include "matchers/hart_matcher.hpp"
#include "metrics/hart_true_peak.hpp"
#include "hart_str.hpp"
#include "hart_utils.hpp"  // decibelsToRatio(), nan()

namespace hart
{

/// @brief Checks whether the audio true-peaks below specific level
/// @details
/// It checks inter-sample peaks by observing oversampled signal, following
/// [ITU-R BS.1770-5](https://www.itu.int/rec/R-REC-BS.1770-5-202311-I/en)
/// guidelines. Some of the implementation choices are exposed by ctor,
/// such as oversampling factor and number of taps in the internal poly-phase
/// IIR filter, as the standard does not specify the exact values.
/// Also, you may pick whether to leave enough headroom due to potential
/// measuring under-shoot via setting `Strictness` option.
///
/// For more versatile expressions, you may also check `truePeak()` metric.
///
/// @ingroup Matchers
template<typename SampleType>
class TruePeaksBelow:
    public Matcher<SampleType, TruePeaksBelow<SampleType>>
{
public:
    /// @brief Number of taps of the internal poly-phase IIR filter
    enum FilterQuality
    {
        low = 12,  //!< 12 taps
        medium = 24,  //!< 24 taps
        high = 96  //!< 96 taps
    };

    /// @brief Strictness to when it comes to decision on whether the signal is below a specific dB TP target
    enum class Strictness
    {
        relaxed,  //!< Numeric tolerance only
        strict  //!< Numeric tolerance + estimated under-read
    };

    /// @brief Creates a matcher for a specific true peak level
    /// @param thresholdDbTP Expected true peak threshold in decibels true peak ("dB TP")
    /// @param oversampling Oversampling ratio, see @ref Oversampling
    /// @param filterQuality Resolution of the internal IIR filters.
    /// "low" is in line with ITU-R BS.1770 measuring recommendations, "high" a bit closer to what mass-produced
    /// DACs implement (although still a rough approximate), "medium" is somewhere in between.
    /// @param strictness Whether to take estimated true peaks at face value, or also take potential
    /// under-read in consideration. Under-read estimation is calculated according to ITU-R BS.1770-5, Annex 2,
    /// Attachement 1
    /// ([See page 21 here](https://www.itu.int/dms_pubrec/itu-r/rec/bs/R-REC-BS.1770-5-202311-I!!PDF-E.pdf)).
    /// For "strict" setting, worst case will be assumed, and additional headroom equal to maximum under-shoot
    /// at $$f_norm = 0.45$$ will be required for the audio to pass this matcher's requirement.
    /// @param numericToleranceLinear Absolute tolerance for comparing frames, in linear domain (not decibels)
    TruePeaksBelow (
        double thresholdDbTP,
        Oversampling oversamplingRatio = Oversampling::x4,
        FilterQuality filterQuality = FilterQuality::low,
        Strictness strictness = Strictness::relaxed,
        double numericToleranceLinear = 1e-3
        ) :

        m_thresholdDbTP (thresholdDbTP),
        m_thresholdLinear (static_cast<SampleType> (decibelsToRatio (thresholdDbTP) + numericToleranceLinear)),
        m_oversamplingRatio (oversamplingRatio),
        m_filterQuality (static_cast<typename TruePeak<SampleType>::FilterQuality> (filterQuality)),
        m_strictness (strictness),
        m_numericToleranceLinear (static_cast<SampleType> (numericToleranceLinear)),
        truePeakEstimator (oversamplingRatio, m_filterQuality)
    {
    }

    void prepare (double sampleRateHz, size_t /* numInputChannels */, size_t numOutputChannels, size_t /* maxBlockSizeFrames */) override
    {
        const size_t numActiveChannels = this->getChannelFlags().numTrue();
        hassert (numActiveChannels > 0);  // Zero active channels is technically still valid, but definitely weird
        hassert (numActiveChannels <= numOutputChannels);

        truePeakEstimator.prepare (sampleRateHz, numActiveChannels);
        m_sampleRateHz = sampleRateHz;  // Just for readable failure details
    }

    void reset() override
    {
        m_TruePeakLinear = (SampleType) 0;
        truePeakEstimator.reset();
    }

    bool match (AnalysisContext<SampleType> context) override
    {
        const AudioBuffer<SampleType>& observedOutputAudio = context.outputAudio();

        const ChannelFlags channelFlags = this->getChannelFlags();
        const size_t numChannels = observedOutputAudio.getNumChannels();
       // hassert (numChannels <= observedOutputAudio.getNumFrames());

        std::vector<size_t> channels;
        channels.reserve (channelFlags.numTrue());

        for (size_t channel = 0; channel < numChannels; ++channel)
            if (channelFlags[channel] == true)
                channels.push_back (channel);

        const typename TruePeak<SampleType>::Result estimatorResult = truePeakEstimator.estimate (observedOutputAudio, std::move (channels));
        m_TruePeakLinear = estimatorResult.valueLinear;
        m_failedChannel = estimatorResult.channel;
        m_failedFrame = estimatorResult.frame;

        const bool matcherResult = ! isAboveRequiredThreshold (m_TruePeakLinear);
        m_hasPassed &= matcherResult;
        return matcherResult;
    }

    bool canOperatePerBlock() const override
    {
        // It's more useful to display the highest peak of
        // the entire audio clip, when the matcher fails
        return false;
    }

    MatcherFailureDetails getFailureDetails() const override
    {
        const double maximumUnderReadLinear = static_cast<double> (truePeakEstimator.getMaximumUnderReadLinear());
        hassert (! floatsEqual (maximumUnderReadLinear, 0.0));
        hassert (maximumUnderReadLinear <= 1.0);
        const double maximumUnderReadDb = ratioToDecibels (maximumUnderReadLinear);

        const double observedPeakDbTP = ratioToDecibels (m_TruePeakLinear);
        const double maximumMormalizedFrequencyHz = m_fNorm * m_sampleRateHz / 2;

        std::ostringstream detailsStream;
        detailsStream
            << "Observed audio true peak is "
            << linPrecision << m_TruePeakLinear << " ("
            << dbPrecision << observedPeakDbTP << " dB TP), at frame "
            << std::setprecision (4) << m_failedFrame
            << "\nMaximum under-read at f_norm = " << std::setprecision (2) << m_fNorm
            << hzPrecision << " (" << maximumMormalizedFrequencyHz << " Hz) is " << maximumUnderReadDb << " dB, "
            << (m_strictness == Strictness::relaxed ? "not " : "") << "taken into account";

        MatcherFailureDetails details;
        details.frame = hart::roundToSizeT (m_failedFrame);  // TODO: Report floating point value
        details.channel = m_failedChannel;
        details.description = detailsStream.str();

        return details;
    }

    void represent (std::ostream& stream) const override
    {
        stream << "TruePeaksBelow ("
            << dbPrecision << m_thresholdDbTP << "_dBTP, "
            << m_oversamplingRatio << ", "
            << m_filterQuality << ", "
            << m_strictness << ", "
            << linPrecision << m_numericToleranceLinear << ')';
    }

    friend std::ostream& operator<< (std::ostream& os, Strictness strictness)
    {
        return os << "Strictness::" << (strictness == Strictness::relaxed ? "relaxed" : "strict");
    }

    friend std::ostream& operator<< (std::ostream& os, FilterQuality filterQuality)
    {
        os << "FilterQuality::";

        switch (filterQuality)
        {
            case FilterQuality::low : os << "low"; break;
            case FilterQuality::medium : os << "medium"; break;
            case FilterQuality::high : os << "high"; break;
        }

        return os;
    }

private:
    static constexpr double m_fNorm = 0.45;  // It's a ratio, not Hz
    const double m_thresholdDbTP;
    const SampleType m_thresholdLinear;
    const Oversampling m_oversamplingRatio;
    const typename TruePeak<SampleType>::FilterQuality m_filterQuality;
    const Strictness m_strictness;
    const SampleType m_numericToleranceLinear;
    double m_sampleRateHz = hart::nan<double>();
    SampleType m_TruePeakLinear = static_cast<SampleType>(0);
    TruePeak<SampleType> truePeakEstimator;

    bool m_hasPassed = true;
    double m_failedFrame = hart::nan<double>();
    size_t m_failedChannel = 0;

    // Outer vector = channels
    // Inner vector = ring buffer of previous samples
    // History index is shared since all channels advance in lockstep
    std::vector<std::vector<SampleType>> m_history;
    size_t m_historyIndex = 0;
    size_t m_offsetFrames = 0;

    // TODO: Flatten m_phaseCoefficients and m_history to 1D vectors?
    std::vector<std::vector<SampleType>> m_phaseCoefficients;

    inline bool isAboveRequiredThreshold (SampleType rectifiedPeakLinear)
    {
        return m_strictness == Strictness::strict
            ? rectifiedPeakLinear / truePeakEstimator.getMaximumUnderReadLinear() > m_thresholdLinear
            : rectifiedPeakLinear > m_thresholdLinear;
    }
};

HART_MATCHER_DECLARE_ALIASES_FOR (TruePeaksBelow)

}  // namespace hart
