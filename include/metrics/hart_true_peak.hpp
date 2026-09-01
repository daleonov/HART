#pragma once

#include <algorithm>  // fill()
#include <vector>

#include "hart_audio_buffer.hpp"
#include "hart_exceptions.hpp"  // hassert()
#include "hart_channel_flags.hpp"
#include "hart_slice.hpp"
#include "hart_utils.hpp"  // Oversampling, floatsEqual(), decibelsToRatio(), nan()

namespace hart
{

/// @brief Multi-channel True peak estimator, based on ITU-R BS.1770 measuring recommendations
/// @details Meant to be used as an implementation for `truePeak()` metric and `TruePeaksBelow` matcher.
/// @private
template<typename SampleType>
class TruePeak
{
public:
    /// @brief Number of taps of the internal poly-phase FIR filter
    enum FilterQuality
    {
        low = 12,  //!< 12 taps
        medium = 24,  //!< 24 taps
        high = 96  //!< 96 taps
    };

    struct Result
    {
        const SampleType valueLinear;
        const size_t channel;
        const double frame;
    };

    TruePeak (Oversampling oversamplingRatio = Oversampling::x4, FilterQuality filterQuality = FilterQuality::low) :
        m_oversamplingRatio (oversamplingRatio),
        m_filterQuality (filterQuality),
        m_maximumUnderReadLinear (calculateMaximumUnderReadLinear())
    {
        hassert (! floatsEqual (m_maximumUnderReadLinear, (SampleType) 0));
        hassert (m_maximumUnderReadLinear <= (SampleType) 1);
    }

    /// @param numActiveChannels Number of channels that we want to measure, e. g. the channels
    /// selected via Matcher::atChannels(). Not the total number of channels in the audio buffers.
    void prepare (double sampleRateHz, size_t numActiveChannels)
    {
        // TODO: If instantiated by a matcher, we can skip allocating history
        // for channels that should be skipped

        m_history.assign (numActiveChannels, std::vector<SampleType> (getTapsPerPhase(), (SampleType) 0));
        m_historyTapIndex = 0;
        m_offsetFrames = 0;
        m_TruePeakLinear = (SampleType) 0;
        buildPhaseCoefficients();

        m_sampleRateHz = sampleRateHz;  // Just for readable failure details
    }

    void reset()
    {
        for (std::vector<SampleType>& channelHistory : m_history)
            std::fill (channelHistory.begin(), channelHistory.end(), (SampleType) 0);

        m_historyTapIndex = 0;
        m_offsetFrames = 0;
        m_TruePeakLinear = (SampleType) 0;
    }

    Result estimate (const AudioBuffer<SampleType>& observedOutputAudio, std::vector<size_t>&& channels, Slice slice = Slice::whole())
    {
        if (channels.empty() || slice.isEmpty())
            return { hart::nan<SampleType>(), 0, hart::nan<double>()};

        hassert (m_history.size() == channels.size());
        hassert (m_history[0].size() == getTapsPerPhase());

        const auto sliceFrameIndices = observedOutputAudio.getFrameIndices (slice);
        const size_t sliceStart = sliceFrameIndices.first;
        const size_t sliceStop = sliceFrameIndices.second;
        hassert (sliceStop > sliceStart);
        hassert (sliceStop - sliceStart != 0);
        hassert (sliceStop <= observedOutputAudio.getNumFrames());
        
        const size_t oversamplingRatio = getRatio();
        SampleType truePeakValueLinear = (SampleType) 0;
        size_t truePeakChannel = 0;
        double truePeakFrame = 0.0;

        for (size_t frame = sliceStart; frame < sliceStop; ++frame)
        {
            size_t historyChannelIndex = 0;

            for (size_t channel : channels)
            {
                hassert (channel < observedOutputAudio.getNumChannels());
                hassert (historyChannelIndex < m_history.size())

                m_history[historyChannelIndex][m_historyTapIndex] = observedOutputAudio[channel][frame];

                for (size_t phase = 0; phase < oversamplingRatio; ++phase)
                {
                    const SampleType oversampledPeakLinear = evaluatePolyphaseFIR (historyChannelIndex, phase);
                    const SampleType rectifiedPeakLinear = std::abs (oversampledPeakLinear);

                    if (rectifiedPeakLinear > truePeakValueLinear)
                    {
                        truePeakValueLinear = rectifiedPeakLinear;
                        truePeakChannel = channel;
                        truePeakFrame =
                            static_cast<double> (m_offsetFrames) +
                            static_cast<double> (phase) / static_cast<double> (oversamplingRatio);
                    }

                }

                ++historyChannelIndex;
            }

            m_historyTapIndex = (m_historyTapIndex + 1) % getTapsPerPhase();
            ++m_offsetFrames;
        }

        return {truePeakValueLinear, truePeakChannel, truePeakFrame};
    }

    SampleType getMaximumUnderReadLinear() const
    {
        return m_maximumUnderReadLinear;
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
    const Oversampling m_oversamplingRatio;
    const FilterQuality m_filterQuality;
    const SampleType m_maximumUnderReadLinear;
    double m_sampleRateHz = hart::nan<double>();
    SampleType m_TruePeakLinear = static_cast<SampleType>(0);

    // Outer vector = channels
    // Inner vector = ring buffer of previous samples
    // History index is shared since all channels advance in lockstep
    std::vector<std::vector<SampleType>> m_history;
    size_t m_historyTapIndex = 0;
    size_t m_offsetFrames = 0;

    // TODO: Flatten m_phaseCoefficients and m_history to 1D vectors?
    std::vector<std::vector<SampleType>> m_phaseCoefficients;

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

    SampleType evaluatePolyphaseFIR (size_t historyChannelIndex, size_t phase) const
    {
        const auto& history = m_history[historyChannelIndex];
        const auto& coeffs = m_phaseCoefficients[phase];

        AccurateSum<SampleType> sum;
        const size_t size = history.size();

        for (size_t tap = 0; tap < coeffs.size(); ++tap)
        {
            const size_t index = (m_historyTapIndex + size - tap) % size;
            sum += history[index] * coeffs[tap];
        }

        return sum.getValue();
    }

    void buildPhaseCoefficients()
    {
        // Windowed-sinc polyphase FIR generator
        // TODO: Implement coefficients caching?

        const size_t oversamplingRatio = getRatio();
        const size_t tapsPerPhase = getTapsPerPhase();
        m_phaseCoefficients.assign (oversamplingRatio, std::vector<SampleType> (tapsPerPhase, (SampleType) 0));

        const double center = static_cast<double> (tapsPerPhase - 1) / 2.0;

        for (size_t phase = 0; phase < oversamplingRatio; ++phase)
        {
            AccurateSum<SampleType> norm;
            const double frac = static_cast<double> (phase) / static_cast<double> (oversamplingRatio);

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

/// @brief Estimates true peak (inter-sample peak) level
/// @details
/// It checks inter-sample peaks by observing oversampled signal, following
/// [ITU-R BS.1770-5](https://www.itu.int/rec/R-REC-BS.1770-5-202311-I/en)
/// guidelines. Some of the implementation choices are exposed via arguments,
/// such as oversampling factor and number of taps in the internal poly-phase
/// FIR filter, as the standard does not specify the exact values.
///
/// Supports values in dB TP (`Unit::dB`) and linear domain (Unit::linear).
/// Operating at default unit (`Unit::native`) will yield values in dB TP.
///
/// Shares the same implementation as `TruePeaksBelow` matcher, but lets you
/// make more versatile expressions.
///
/// @param audioBuffer Buffer to estimate true peaks in
/// @param oversamplingRatio Oversampling for the estimator. Higher OS ratios
/// are expected to result in more accurate estimations.
/// @param filterQuality Represent number of taps for the internal FIR filter.
/// Higher will result in more accurate estimate. Note that even the highest
/// filter quality is way lower than what is used in actual DAC oversamplers,
/// but it's okay, since we're merely estimating here.
/// @ingroup Metrics
template <typename SampleType>
MetricQuery<double> truePeak (
    const AudioBuffer<SampleType>& audioBuffer,
    Oversampling oversamplingRatio = Oversampling::x4,
    typename TruePeak<SampleType>::FilterQuality filterQuality = TruePeak<SampleType>::FilterQuality::low
    )
{
    // Lambda (estimator) will actually own the truePeakEstimator object,
    // but for C++11 compatibility sake, it's easier to pass it by copy
    // as a shared pointer.
    auto truePeakEstimator = std::make_shared<TruePeak<SampleType>> (oversamplingRatio, filterQuality);
    truePeakEstimator->prepare (
        audioBuffer.getSampleRateHz(),
        1  // Will estimate one channel at a time
        );

    MetricQuery<double>::SingleChannelMetricEvaluator evaluator =
        [&audioBuffer, truePeakEstimator]
        (size_t channel, Slice slice, Unit requestedUnit)
        -> double
    {
        hassert (truePeakEstimator != nullptr);
        truePeakEstimator->reset();  // This specific instance has 1 stateful internal channel for every measured audio channel
        const typename TruePeak<SampleType>::Result estimatorResult = truePeakEstimator->estimate (audioBuffer, {{channel}}, slice);
        const double truePeakLinear = static_cast<double> (estimatorResult.valueLinear);

        switch (requestedUnit)
        {
            case Unit::linear: return truePeakLinear;

            case Unit::native:
            case Unit::dB: return hart::ratioToDecibels (truePeakLinear);

            default: HART_THROW_OR_RETURN (hart::UnitError, "Unsupported unit",  hart::nan<double>());
        }
    };

    const size_t numChannels = audioBuffer.getNumChannels();
    return MetricQuery<double> (
        std::move (evaluator),
        numChannels,
        ChannelSubsets::allChannels (numChannels)
    );
}

}  // namespace hart
