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
    // TODO: Document this helper!
    inline double closestCoherentFrequencyHz (
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
        const size_t maxFundamentalBin = (nyquistBin - 1) / static_cast<size_t> (maxHarmonic);

        const size_t fundamentalBin = hart::clamp (
            roundToSizeT (desiredFrequencyHz / binWidthHz),
            (size_t) 1,
            maxFundamentalBin
            );

        const double fundamentalFrequencyHz = static_cast<double> (fundamentalBin) * binWidthHz;
        hassert (floatsNotEqual (fundamentalFrequencyHz, 0.0));
        return fundamentalFrequencyHz;
    }
}  // namespace THD


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
/// the measured THD. You may use the provided helpers to ensure that, namely
/// `hart::THD::closestCoherentFrequencyHz()` for sine frequency and
/// `hart::previousPowerOfTwo()` or `hart::nextPowerOfTwo()` for the render
/// duration in frames.
///
/// Since Spectrum uses a power-of-two FFT size, a convenient way to satisfy
/// these requirements would be:
///
/// 1. Choose a power-of-two render length close to the desired duration
/// 2. Use hart::THD::closestCoherentFrequencyHz() to choose the FFT-bin
/// frequency closest to the desired fundamental frequency, so that it falls
/// perfectly in the middle of the FFT bin.
///
/// Example:
///
/// @code
/// // Assuming you have specific render duration and input signal frequency in mind:
/// constexpr double desiredRenderDurationSeconds = 100_ms; // Or any other duration you want
/// constexpr double desiredFrequencyHz = 1_kHz; // Or any other frequency you want
///
/// // Choose a power-of-two render length so Spectrum does not need to
/// // zero-pad the rendered signal for the FFT
/// const size_t desiredRenderDurationFrames = hart::roundToSizeT (desiredRenderDurationSeconds * sampleRateHz);
/// const size_t renderDurationFrames = hart::previousPowerOfTwo (desiredRenderDurationFrames);
/// const double renderDurationSeconds = static_cast<double> (renderDurationFrames) / sampleRateHz;
///
/// // Choose the frequency closest to the requested frequency that lies
/// // exactly at the centre of an FFT bin.
/// const double coherentFrequencyHz =
///     hart::THD::closestCoherentFrequencyHz (
///         desiredFrequencyHz,
///         renderDurationFrames,
///         sampleRateHz
///         );
///
/// processAudioWith (SomeDSP())
///     .withInputSignal (SineWave (coherentFrequencyHz))  // Corrected frequency...
///     .withDuration (renderDurationSeconds)  // ...and corrected render duration
///     .expectTrue (
///         [coherentFrequencyHz] (const auto& output)
///         {
///             return HART_FLOAT_EQ (
///                 hart::thd (hart::Spectrum (output), coherentFrequencyHz).get(),
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
/// @param fundamentalFrequencyHz Frequency of the fundamental in Hz. It must
/// correspond exactly to an FFT bin frequency. Use `hart::THD::closestCoherentFrequencyHz()`
/// to obtain an appropriate value.
/// @param numHarmonics Maximum harmonic number to include in the measurement.
/// Harmonics at or above Nyquist are ignored.
/// @return A MetricQuery containing THD as a linear amplitude ratio. May return `NaN`.
inline MetricQuery<double> thd (const Spectrum& spectrum, double fundamentalFrequencyHz, int numHarmonics = 10)
{
    typename MetricQuery<double>::SingleChannelMetricEvaluator evaluator =
        [&spectrum, fundamentalFrequencyHz, numHarmonics]
        (size_t channel, const Slice& slice, Unit requestedUnit)
        -> double
    {
        hassert (channel < spectrum.getNumChannels());
        hassert (! std::isnan (spectrum.getSampleRateHz()));

        // TODO: Add percent unit support?
        if (requestedUnit != Unit::native && requestedUnit != Unit::linear)
            HART_THROW_OR_RETURN (hart::UnitError, "Unsupported unit", hart::nan<double>());

        if (slice.type != Slice::Type::whole)
            HART_THROW_OR_RETURN (hart::ValueError, "Cannot calculate THD of a portion of spectrum", hart::nan<double>());

        const size_t fundamentalBin = spectrum.findClosestBin (fundamentalFrequencyHz);

        // Might be a bit too strict, but for accurate THD measurement the fundamental must be coherent
        if (floatsNotEqual (fundamentalFrequencyHz, spectrum.getBinFrequencyHz (fundamentalBin)))
            HART_THROW_OR_RETURN (hart::ValueError, "Fundamental frequency should be in the middle of the bin. Use closestCoherentFrequencyHz() to calculate appropriate signal frequency.", hart::nan<double>());

        const double fundamentalPower = std::norm (spectrum.getBinValue (channel, fundamentalBin));
        
        if (fundamentalPower < 1e-15)
            return std::numeric_limits<double>::infinity();
        
        const double nyquistFrequencyHz = spectrum.getSampleRateHz() / 2.0;
        AccurateSum<double> harmonicPowerSum;
        
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
