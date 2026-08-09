#pragma once

#include <complex>  // norm()
#include <cmath>  // floor(), round(), sqrt()

#include "hart_accurate_sum.hpp"
#include "hart_exceptions.hpp"
#include "hart_utils.hpp"  // nextPowerOfTwo(), roundToSizeT(), floatsNotEqual(), clamp()

namespace hart
{

namespace THD
{

struct ExperimentSetup
{
    const double frequencyHz;
    const double durationSeconds;
    const size_t durationFrames;
    const int numHarmonics;

    ExperimentSetup (const ExperimentSetup&) = default;
    ExperimentSetup (ExperimentSetup&&) = default;
    ExperimentSetup& operator= (const ExperimentSetup&) = delete;
    ExperimentSetup& operator= (ExperimentSetup&&) = delete;
    ~ExperimentSetup() = default;

private:
    friend class ExperimentSetupTuner;

    // Supposed to be constructed only via ExperimentSetupTuner
    ExperimentSetup (double frequencyHz_, double durationSeconds_, size_t durationFrames_, int numHarmonics_) :
        frequencyHz (frequencyHz_),
        durationSeconds (durationSeconds_),
        durationFrames (durationFrames_),
        numHarmonics (numHarmonics_)
    {
    }

    /// @brief Invalid default-constructed structure
    /// @details Supposed to be constructed only when builder encouters any runtime errors
    ExperimentSetup() :
        frequencyHz (hart::nan<double>()),
        durationSeconds (hart::nan<double>()),
        durationFrames (0),
        numHarmonics (0)
    {
    }
};

class ExperimentSetupTuner
{
public:
    ExperimentSetupTuner& withFrequency (double desiredFrequencyHz)
    {
        if (desiredFrequencyHz < 0.0 || floatsEqual (desiredFrequencyHz, 0.0) || std::isnan (desiredFrequencyHz))
            HART_THROW_OR_RETURN (hart::ValueError, "Invalid fundamental frequency", *this);

        m_desiredFrequencyHz = desiredFrequencyHz;
        return *this;
    }

    ExperimentSetupTuner& withSampleRate (double sampleRateHz)
    {
        if (sampleRateHz <= 0.0 || floatsEqual (sampleRateHz, 0.0) || std::isnan (sampleRateHz))
            HART_THROW_OR_RETURN (hart::SampleRateError, "Invalid sample rate", *this);
    
        m_sampleRateHz = sampleRateHz;
        return *this;
    }

    ExperimentSetupTuner& withDuration (double desiredDurationSeconds)
    {
        if (desiredDurationSeconds < 0.0 || floatsEqual (desiredDurationSeconds, 0.0) || std::isnan (desiredDurationSeconds))
            HART_THROW_OR_RETURN (hart::ValueError, "Invalid duration", *this);

        m_desiredDurationSeconds = desiredDurationSeconds;
        m_durationSpecifiedInFrames = false;
        return *this;
    }

    ExperimentSetupTuner& withNumFrames (size_t desiredDurationFrames)
    {
        if (desiredDurationFrames == 0)
            HART_THROW_OR_RETURN (hart::ValueError, "Invalid duration", *this);
        
        m_desiredDurationFrames = desiredDurationFrames;
        m_durationSpecifiedInFrames = true;
        return *this;
    }

    ExperimentSetupTuner& withNumHarmonics (int desiredNumHarmonics)
    {
        if (desiredNumHarmonics < 2)
            HART_THROW (hart::ValueError, "Invalid number of harmonics");

        m_desiredNumHarmonics = desiredNumHarmonics;
        return *this;
    }

    ExperimentSetup tune() const
    {
        const size_t requestedDurationFrames =
            m_durationSpecifiedInFrames
                ? m_desiredDurationFrames
                : roundToSizeT (m_desiredDurationSeconds * m_sampleRateHz);

        // TODO: Make closestPowerOfTwo()?
        const size_t tunedDurationFrames = hart::previousPowerOfTwo (requestedDurationFrames);

        if (tunedDurationFrames < 8)
            HART_THROW_OR_RETURN (hart::ValueError, "Experiment duration is too short for THD measurement", {});

        const double tunedDurationSeconds = static_cast<double> (tunedDurationFrames) / m_sampleRateHz;
        const size_t nyquistBin = tunedDurationFrames / 2;

        // At least bins 1..numHarmonics must fit strictly below Nyquist.
        const int maxValidNumHarmonics = static_cast<int> (nyquistBin) - 1;

        const int tunedNumHarmonics = std::min (m_desiredNumHarmonics, maxValidNumHarmonics);
        hassert (tunedNumHarmonics >= 2);

        const double tunedFrequencyHz =
            closestCoherentFrequencyHz (
                m_desiredFrequencyHz,
                tunedDurationFrames,
                m_sampleRateHz,
                tunedNumHarmonics
            );

        return {
            tunedFrequencyHz,
            tunedDurationSeconds,
            tunedDurationFrames,
            tunedNumHarmonics
        };
    }

private:
    double m_desiredFrequencyHz = 1000.0;
    double m_sampleRateHz = CLIConfig::getInstance().getDefaultSampleRateHz();
    double m_desiredDurationSeconds = CLIConfig::getInstance().getDefaultRenderDurationSeconds();
    size_t m_desiredDurationFrames = 0;
    int m_desiredNumHarmonics = 10;
    bool m_durationSpecifiedInFrames = false;

    static double closestCoherentFrequencyHz (
        double desiredFrequencyHz,
        size_t fftSizeFrames,
        double sampleRateHz,
        int maxHarmonic = 10
        )
    {
        const double nan = hart::nan<double>();

        if (! isPowerOfTwo (fftSizeFrames))
            HART_THROW_OR_RETURN (hart::SizeError, "FFT size is expected to be a power of 2", nan);

        if (sampleRateHz < 0.0 || floatsEqual (sampleRateHz, 0.0) || std::isnan (sampleRateHz))
            HART_THROW_OR_RETURN (hart::SampleRateError, "Invalid sample rate", nan);

        if (desiredFrequencyHz < 0.0 || floatsEqual (desiredFrequencyHz, 0.0) || std::isnan (desiredFrequencyHz))
            HART_THROW_OR_RETURN (hart::ValueError, "Invalid sample rate", nan);

        if (maxHarmonic < 2)
            HART_THROW_OR_RETURN (hart::ValueError, "Invalid max harmonic number", nan);

        const double binWidthHz = sampleRateHz / static_cast<double> (fftSizeFrames);
        const size_t nyquistBin = fftSizeFrames / 2;

        if (static_cast<size_t> (maxHarmonic) >= nyquistBin)
            HART_THROW_OR_RETURN (hart::ValueError, "FFT size is too small for the requested number of harmonics", nan);

        const size_t maxFundamentalBin = (nyquistBin - 1) / static_cast<size_t> (maxHarmonic);

        hassert (maxFundamentalBin >= 1);
        hassert (maxFundamentalBin * static_cast<size_t> (maxHarmonic) < nyquistBin);

        const size_t fundamentalBin = hart::clamp (
            roundToSizeT (desiredFrequencyHz / binWidthHz),
            (size_t) 1,
            maxFundamentalBin
            );

        const double fundamentalFrequencyHz = static_cast<double> (fundamentalBin) * binWidthHz;
        hassert (floatsNotEqual (fundamentalFrequencyHz, 0.0));
        return fundamentalFrequencyHz;
    }
};

}  // namespace hart::THD


/// @brief Calculates the total harmonic distortion (THD) of a spectrum.
///
/// THD is calculated as the RMS sum of the powers of the harmonics relative
/// to the power of the fundamental:
///
/// @f[
/// \mathrm{THD}
/// = \sqrt{
///     \frac{\sum_{h=2}^{H} |X[h k_1]|^2}
///          {|X[k_1]|^2}
///   }
/// @f]
///
/// (THD = sqrt(sum(norm(harmonic bins)) / norm(fundamental bin))),
///
/// where @f$ k_1 @f$ is the FFT bin containing the fundamental, and @f$ H @f$
/// is the maximum harmonic number requested. Harmonics at or above the
/// Nyquist frequency are ignored.
///
/// For an accurate measurement, the input should be a pure sine whose
/// frequency lies exactly at the centre of an FFT bin. The analysed signal
/// should also contain exactly the same number of frames as the FFT, without
/// zero padding. Otherwise, truncation and zero padding cause spectral
/// leakage, which may appear as harmonic energy and artificially increase
/// the measured THD.
///
/// To ensure those conditions are met, you're expected to obtain a "tuned"
/// experiment setup, obtained through hart::THD::ExperimentSetupTuner,
/// which will snap all of your desired experiment parameters to the values
/// optimized for no-spill FFT. Those will be the values you're supposed to
/// run the entire test render with.
///
/// Example:
///
/// @code
/// // Assuming you have specific render duration and input signal frequency in mind:
/// constexpr double desiredRenderDurationSeconds = 100_ms; // Or any other duration you want
/// constexpr double desiredFrequencyHz = 1_kHz; // Or any other frequency you want
/// 
/// // Snap the desired parameters to optimal values
/// const hart::THD::ExperimentSetup setup = hart::THD::ExperimentSetupTuner()
///     .withFrequency (frequencyHz)
///     .withDuration (desiredRenderDurationSeconds)
///     .tune();
///
/// processAudioWith (SomeDSP())
///     .withInputSignal (SineWave (setup.frequencyHz))  // Corrected frequency...
///     .withDuration (setup.durationSeconds)  // ...and corrected render duration
///     .expectTrue (
///         [setup] (const hart::AudioBuffer<float>& output)
///         {
///             return HART_FLOAT_EQ (
///                 hart::thd (hart::Spectrum (output), setup).get(),
///                 0.0,
///                 1e-8
//                  );
///         },
///         "THD ~= 0")
///     .process();
/// @endcode
///
/// @param spectrum Spectrum of the output signal to analyse, assuming the input
/// was a pure sine wave
/// @param experimentSetup Optimized experiment setup values obtained through
/// hart::THD::ExperimentSetupTuner().
/// @return A MetricQuery containing THD as a linear amplitude ratio. May return `NaN`.
inline MetricQuery<double> thd (const Spectrum& spectrum, THD::ExperimentSetup experimentSetup)
{
    typename MetricQuery<double>::SingleChannelMetricEvaluator evaluator =
        [&spectrum, experimentSetup]
        (size_t channel, const Slice& slice, Unit requestedUnit)
        -> double
    {
        hassert (channel < spectrum.getNumChannels());
        hassert (! std::isnan (spectrum.getSampleRateHz()));

        const double nan = hart::nan<double>();
        const double fundamentalFrequencyHz = experimentSetup.frequencyHz;

        // Make sure your test's render time is exactly experimentSetup.durationSeconds (or experimentSetup.durationFrames)
        if (spectrum.getFFTSize() != experimentSetup.durationFrames)
            HART_THROW_OR_RETURN (hart::ValueError, "FFT size doesn't match duration in the provided experiment setup", nan);

        if (experimentSetup.durationFrames == 0 || floatsEqual (experimentSetup.durationSeconds, 0.0))
            HART_THROW_OR_RETURN (hart::SizeError, "Experiment setup should not have duration of zero - nothing to analyze", nan);

        const double experimentSetupSampleRateHz = static_cast<double> (experimentSetup.durationFrames) / experimentSetup.durationSeconds;

        // The duration of input signal should be exactly experimentSetup.durationSeconds and experimentSetup.durationFrames
        if (floatsNotEqual (spectrum.getSampleRateHz(), experimentSetupSampleRateHz))
            HART_THROW_OR_RETURN (hart::ValueError, "Spectrum's sample rate doesn't match one derived from the provided experiment setup instance", nan);

        // TODO: Add percent unit support?
        if (requestedUnit != Unit::native && requestedUnit != Unit::linear)
            HART_THROW_OR_RETURN (hart::UnitError, "Unsupported unit", nan);

        if (slice.type != Slice::Type::whole)
            HART_THROW_OR_RETURN (hart::ValueError, "Cannot calculate THD of a portion of spectrum", nan);

        const size_t fundamentalBin = spectrum.findClosestBin (fundamentalFrequencyHz);

        // The input signal in the experiment should be a sine wave at exactly experimentSetup.frequencyHz
        if (floatsNotEqual (fundamentalFrequencyHz, spectrum.getBinFrequencyHz (fundamentalBin)))
            HART_THROW_OR_RETURN (hart::ValueError, "Fundamental frequency in the provided spectrum doesn't match one in experiment setup", nan);

        // Might be a bit too strict, but for accurate THD measurement the fundamental must be coherent
        if (floatsNotEqual (fundamentalFrequencyHz, spectrum.getBinFrequencyHz (fundamentalBin)))
            HART_THROW_OR_RETURN (hart::ValueError, "Fundamental frequency should be in the middle of the bin. Use closestCoherentFrequencyHz() to calculate appropriate signal frequency.", hart::nan<double>());

        const double fundamentalPower = std::norm (spectrum.getBinValue (channel, fundamentalBin));
        
        if (fundamentalPower < 1e-15)
            return std::numeric_limits<double>::infinity();
        
        const double nyquistFrequencyHz = spectrum.getSampleRateHz() / 2.0;
        AccurateSum<double> harmonicPowerSum;

        const int numHarmonics = experimentSetup.numHarmonics;
        hassert (numHarmonics > 2);
        
        for (int harmonic = 2; harmonic <= numHarmonics; ++harmonic)
        {
            const double harmonicFrequencyHz = fundamentalFrequencyHz * harmonic;

            if (harmonicFrequencyHz >= nyquistFrequencyHz)
                break;
                
            // TODO: Probably just do "harmonicBin = fundamentalBin * harmonic" here
            const size_t harmonicBin = spectrum.findClosestBin (harmonicFrequencyHz);
            harmonicPowerSum += std::norm (spectrum.getBinValue (channel, harmonicBin));
        }
        
        return std::sqrt (harmonicPowerSum / fundamentalPower);
    };

    const size_t numChannels = spectrum.getNumChannels();
    return MetricQuery<double> (
        std::move (evaluator),
        numChannels,
        ChannelSubsets::allChannels (numChannels)
    );
}


}  // namespace hart
