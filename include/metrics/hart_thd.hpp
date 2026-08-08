#pragma once

#include <complex>  // norm()
#include <cmath>  // floor(), round(), sqrt()

#include "hart_accurate_sum.hpp"
#include "hart_utils.hpp"  // nextPowerOfTwo(), roundToSizeT(), floatsNotEqual()

namespace hart
{

namespace THD
{
    inline double closestCoherentFrequencyHz (
        double desiredFrequencyHz,
        double signalDurationSeconds,
        double sampleRateHz,
        int maxHarmonic = 10
        )
    {
        const size_t signalDurationFrames = roundToSizeT (signalDurationSeconds * sampleRateHz);
        const double fftSizeFrames = static_cast<double> (nextPowerOfTwo (signalDurationFrames));
        const double binWidth = sampleRateHz / fftSizeFrames;
        const double mMax = std::floor ((fftSizeFrames / 2.0) / maxHarmonic);
        const double m = hart::clamp (std::round (desiredFrequencyHz / binWidth), 1.0, mMax);
        return m * binWidth;
    }
}  // namespace THD

inline MetricQuery<double> thd (const Spectrum& spectrum, double fundamentalFrequencyHz, int numHarmonics = 10)
{
    typename MetricQuery<double>::SingleChannelMetricEvaluator evaluator =
        [&spectrum, fundamentalFrequencyHz, numHarmonics]
        (size_t channel, const Slice& slice, Unit requestedUnit)
        -> double
    {
        hassert (channel < spectrum.getNumChannels());
        hassert (! std::isnan (spectrum.getSampleRateHz()));

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
                
            const size_t harmonicBin = spectrum.findClosestBin (harmonicFrequencyHz);
            hassert (harmonicFrequencyHz)

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
