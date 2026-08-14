#pragma once

// Config for r8brain
#define R8B_IPP 0  // Don't want extra dependencies it comes with
#define R8B_PFFFT 0  // Don't want this extra dependency
#define R8B_PFFFT_DOUBLE 0  // Don't want this extra dependency
#define R8B_FASTTIMING 0  // On the fence here, but doesn't seem to have much of an impact either way
#define R8B_EXTFFT 1  // We don't mind extra latency

#include "dependencies/r8brain-free-src/CDSPResampler.h"
#include "hart_audio_buffer.hpp"
#include "hart_utils.hpp"  // roundToSizeT()

namespace hart
{

/// @brief Resamples audio buffers
/// @details Uses r8brain under the hood, a sample rate converter designed by Aleksey Vaneev of Voxengo
/// @note Intended to be used internally by containers like AudioBuffer or ImpulseResponse, but you can use
/// it too, if you really want to. But it's encouraged to use those containers' methods, whenever appropriate.
/// @ingroup Utils
template <typename SampleType>
static void resample (const SampleType* sourceData, size_t sourceSizeFrames, double sourceSampleRateHz, SampleType* destinationData, size_t destinationCapacityFrames, double destinationSampleRateHz)
{
    if (sourceData == nullptr)
        HART_THROW_OR_RETURN_VOID (NullPointerError, "sourceData pointer is nullptr");

    if (destinationData == nullptr)
        HART_THROW_OR_RETURN_VOID (NullPointerError, "destinationData pointer is nullptr");

    if (sourceSizeFrames == 0)
        HART_THROW_OR_RETURN_VOID (SizeError, "Source data length is zero");

    if (destinationCapacityFrames == 0)
        HART_THROW_OR_RETURN_VOID (SizeError, "Destination data capacity is zero");

    if (sourceSampleRateHz < 0.0 || floatsEqual (sourceSampleRateHz, 0.0) || std::isnan (sourceSampleRateHz))
        HART_THROW_OR_RETURN (hart::SampleRateError, "Invalid source sample rate");

    if (destinationSampleRateHz < 0.0 || floatsEqual (destinationSampleRateHz, 0.0) || std::isnan (destinationSampleRateHz))
        HART_THROW_OR_RETURN (hart::SampleRateError, "Invalid destination sample rate");

    const double sampleRatesRatio = destinationSampleRateHz / sourceSampleRateHz;
    const double minDestinationCapacityFrames = roundToSizeT (static_cast<double> (sourceSizeFrames) * destinationSampleRateHz / sourceSampleRateHz);

    if (destinationCapacityFrames < minDestinationCapacityFrames)
        HART_THROW_OR_RETURN_VOID (SizeError, "Destination buffer doesn't have sufficient capacity");

    // It can also be r8b::CDSPResampler instead of r8b::CDSPResampler24, for even better fidelity
    r8b::CDSPResampler24 resampler (sourceSampleRateHz, destinationSampleRateHz, sourceSizeFrames);
    resampler.oneshot (const_cast<SampleType*> (sourceData), static_cast<int> (sourceSizeFrames), destinationData, static_cast<int> (destinationCapacityFrames));
}

}  // namespace hart
