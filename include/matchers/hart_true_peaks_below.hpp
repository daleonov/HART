#pragma once

#include <cmath>  // abs()
#include <sstream>
#include <vector>

#include "hart_accurate_sum.hpp"
#include "matchers/hart_matcher.hpp"
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
/// IIR filter, as the standard doesn not specify the exact values.
/// Also, you may pick whether to leave enough headroom due to potential
/// measuring under-shoot via setting `Strictness` option.
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
    enum Strictness
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
    /// under-read in consideration. Under-read extimation is calculated according to ITU-R BS.1770-5, Annex 2,
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
        m_filterQuality (filterQuality),
        m_strictness (strictness),
        m_numericToleranceLinear (static_cast<SampleType> (numericToleranceLinear)),
        m_maximumUnderReadLinear (calculateMaximumUnderReadLinear())
    {
        hassert (! floatsEqual (m_maximumUnderReadLinear, (SampleType) 0));
        hassert (m_maximumUnderReadLinear <= (SampleType) 1);
    }

    void prepare (double sampleRateHz, size_t /* numInputChannels */, size_t numOutputChannels, size_t /* maxBlockSizeFrames */) override
    {
        m_history.assign (numOutputChannels, std::vector<SampleType> (getTapsPerPhase(), (SampleType) 0));
        m_historyIndex = 0;
        m_offsetFrames = 0;
        m_TruePeakLinear = (SampleType) 0;
        buildPhaseCoefficients();

        m_sampleRateHz = sampleRateHz;  // Just for readable failure details
    }

    void reset() override
    {
        for (std::vector<SampleType>& channelHistory : m_history)
            std::fill (channelHistory.begin(), channelHistory.end(), (SampleType) 0);

        m_historyIndex = 0;
        m_offsetFrames = 0;
        m_TruePeakLinear = (SampleType) 0;
    }

    bool match (AnalysisContext<SampleType> context) override
    {
        const AudioBuffer<SampleType>& observedOutputAudio = context.outputAudio();

        const size_t numChannels = observedOutputAudio.getNumChannels();
        const size_t numFrames = observedOutputAudio.getNumFrames();
        const size_t ratio = getRatio();

        for (size_t frame = 0; frame < numFrames; ++frame)
        {
            for (size_t channel = 0; channel < numChannels; ++channel)
            {
                m_history[channel][m_historyIndex] = observedOutputAudio[channel][frame];

                for (size_t phase = 0; phase < ratio; ++phase)
                {
                    const SampleType oversampledPeakLinear = evaluatePolyphaseFIR (channel, phase);
                    const SampleType rectifiedPeakLinear = std::abs (oversampledPeakLinear);

                    if (rectifiedPeakLinear > m_TruePeakLinear)
                    {
                        m_TruePeakLinear = rectifiedPeakLinear;
                        m_failedChannel = channel;
                        m_failedFrame =
                            static_cast<double> (m_offsetFrames) +
                            static_cast<double> (phase) / static_cast<double> (ratio);
                    }
                }
            }

            advanceHistory();
            ++m_offsetFrames;
        }

        const bool result = ! isAboveRequiredThreshold (m_TruePeakLinear);
        m_hasPassed &= result;
        return result;
    }

    bool canOperatePerBlock() const override
    {
        // It's more useful to display the highest peak of
        // the entire audio clip, when the matcher fails
        return false;
    }

    MatcherFailureDetails getFailureDetails() const override
    {
        const double observedPeakDbTP = ratioToDecibels (m_TruePeakLinear);
        const double maximumUnderReadDb = ratioToDecibels (m_maximumUnderReadLinear);
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
    const FilterQuality m_filterQuality;
    const Strictness m_strictness;
    const SampleType m_numericToleranceLinear;
    const SampleType m_maximumUnderReadLinear;
    double m_sampleRateHz = hart::nan<double>();
    SampleType m_TruePeakLinear = static_cast<SampleType>(0);

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
            ? rectifiedPeakLinear / m_maximumUnderReadLinear > m_thresholdLinear
            : rectifiedPeakLinear > m_thresholdLinear;
    }

    inline SampleType calculateMaximumUnderReadLinear() const
    {
        // ITU-R BS.1770-5, Attachement 1 to Annex 2, Page 21
        // https://www.itu.int/dms_pubrec/itu-r/rec/bs/R-REC-BS.1770-5-202311-I!!PDF-E.pdf

        return static_cast<SampleType> (std::cos (pi * m_fNorm / getRatio()));
    }

    inline size_t getRatio() const
    {
        return static_cast<size_t> (m_oversamplingRatio);
    }

    inline size_t getTapsPerPhase() const
    {
        return static_cast<size_t> (m_filterQuality);
    }

    void advanceHistory()
    {
        ++m_historyIndex;

        if (m_historyIndex >= getTapsPerPhase())
            m_historyIndex = 0;
    }

    SampleType evaluatePolyphaseFIR (size_t channel, size_t phase) const
    {
        const auto& history = m_history[channel];
        const auto& coeffs = m_phaseCoefficients[phase];

        AccurateSum<SampleType> sum;
        const size_t size = history.size();

        for (size_t tap = 0; tap < coeffs.size(); ++tap)
        {
            const size_t index = (m_historyIndex + size - tap) % size;
            sum += history[index] * coeffs[tap];
        }

        return sum.getValue();
    }

    void buildPhaseCoefficients()
    {
        // Windowed-sinc polyphase FIR generator
        // TODO: Implement coefficients caching?

        const size_t ratio = getRatio();
        const size_t tapsPerPhase = getTapsPerPhase();
        m_phaseCoefficients.assign (ratio, std::vector<SampleType> (tapsPerPhase, (SampleType) 0));

        const double center = static_cast<double> (tapsPerPhase - 1) / 2.0;

        for (size_t phase = 0; phase < ratio; ++phase)
        {
            AccurateSum<SampleType> norm;
            const double frac = static_cast<double> (phase) / static_cast<double> (ratio);

            for (size_t tap = 0; tap < tapsPerPhase; ++tap)
            {
                const double x = static_cast<double> (tap) - center - frac;
                const double sinc = floatsEqual (x, 0.0) ? 1.0 : std::sin (pi * x) / (pi * x);

                // Hann window
                const double window =
                    0.5 - 0.5 * std::cos (2.0 * pi * static_cast<double> (tap) / static_cast<double> (tapsPerPhase - 1));

                const SampleType coeff = static_cast<SampleType> (sinc * window);
                m_phaseCoefficients[phase][tap] = coeff;
                norm += coeff;
            }

            const SampleType normValue = norm.getValue();

            // Normalize each phase for unity DC gain
            for (SampleType& c : m_phaseCoefficients[phase])
                c /= normValue;
        }
    }

};

HART_MATCHER_DECLARE_ALIASES_FOR (TruePeaksBelow)

}  // namespace hart
