#include <algorithm>  // min()
#include <array>
#include <random>
#include <cstdint>  // uint_fast32_t

#include "hart.hpp"

using EqualsTo = hart::EqualsTo<float>;
using HardClip = hart::HardClip<float>;
using GainDb = hart::GainDb<float>;
using PeaksAt = hart::PeaksAt<float>;
using hart::processAudioWith;
using Silence = hart::Silence<float>;
using SineWave = hart::SineWave<float>;

HART_TEST ("DSP Chains - Basic Gain")
{
    processAudioWith (GainDb (0_dB))
        .withInputSignal (SineWave().followedBy (GainDb (-3_dB)))
        .expectTrue (PeaksAt (-3_dB))
        .process();

    processAudioWith (GainDb (0_dB))
        .withInputSignal (SineWave() >> GainDb (-3_dB))
        .expectTrue (PeaksAt (-3_dB))
        .process();

    processAudioWith (GainDb (-2_dB))
        .withInputSignal (SineWave() >> GainDb (-1_dB))
        .expectTrue (PeaksAt (-3_dB))
        .expectTrue (EqualsTo (SineWave() >> GainDb (-3.0_dB)))
        .expectFalse (EqualsTo (SineWave() >> GainDb (-3.1_dB)))
        .process();
}

HART_TEST ("DSP Chains - Order Matters")
{
    processAudioWith (GainDb (0_dB))
        .withInputSignal (SineWave() >> HardClip (-3_dB) >> GainDb (+1_dB))
        .expectTrue (PeaksAt (-2_dB))
        .expectFalse (EqualsTo (SineWave() >> GainDb (+1_dB) >> HardClip (-3_dB)))
        .process();

    processAudioWith (GainDb (0_dB))
        .withInputSignal (SineWave() >> GainDb (+1_dB) >> HardClip (-3_dB))
        .expectTrue (PeaksAt (-3_dB))
        .expectFalse (EqualsTo (SineWave() >> HardClip (-3_dB) >> GainDb (+1_dB)))
        .process();
}

template <size_t ArraySize>
std::array<double, ArraySize> generateRandomValues (uint_fast32_t seed, double min, double max)
{
    std::mt19937 rng (seed);
    std::uniform_real_distribution<double> dist (min, max);
    std::array<double, ArraySize> values;
    double sum = 0;

    for (auto& value : values)
    {
        value = dist(rng);
        sum += value;
    }

    return values;
}

HART_TEST ("DSP Chains - Long Chains")
{
    // 1. A lot of pre-determined gains

    auto signalWithLongFxChainA = SineWave();
    constexpr double gainTargetDb = -10.0;
    constexpr size_t gainInstances = 1000;
    constexpr double gainPerInstanceDb = gainTargetDb / gainInstances;

    for (size_t i = 0; i < gainInstances; ++i)
        signalWithLongFxChainA >> GainDb (gainPerInstanceDb);

    processAudioWith (GainDb (0_dB))
        .withInputSignal (signalWithLongFxChainA)
        .withDuration (20_ms)  // A lot of DSP instances to process, so cutting some corners here
        .expectTrue (PeaksAt (gainTargetDb))
        .process();

    // 2. Random gains

    auto signalWithLongFxChainB = SineWave();
    const uint_fast32_t seed = hart::CLIConfig::get().getRandomSeed();
    const auto gainsDb = generateRandomValues<1000> (seed, -1.0, 1.0);  // A lot of values, but small ones, to avoid large accumulated gains
    double gainTotalDb = 0.0;

    for (const double gainDb : gainsDb)
    {
        signalWithLongFxChainB >> GainDb (gainDb);
        gainTotalDb += gainDb;
    }

    processAudioWith (GainDb (0_dB))
        .withInputSignal (signalWithLongFxChainB)
        .expectTrue (PeaksAt (gainTotalDb))
        .process();

    // 3. Random tresholds

    auto signalWithLongFxChainC = SineWave();
    const auto tresholdsDb = generateRandomValues<50> (seed + 1, -30.0, 0.0);  // Too many items, and we'll always end up getting too close to low end of the range
    double expectedPeakDb = 0.0;

    for (const double gainDb : tresholdsDb)
    {
        signalWithLongFxChainC >> HardClip (gainDb);
        expectedPeakDb = std::min (gainDb, expectedPeakDb);
    }

    processAudioWith (GainDb (0_dB))
        .withInputSignal (signalWithLongFxChainC)
        .expectTrue (PeaksAt (expectedPeakDb))
        .process();
}
