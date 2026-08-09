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
