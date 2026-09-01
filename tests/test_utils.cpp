#include <limits>  // double: denorm_min(), min()
#include <vector>
#include <unordered_set>

#include "hart.hpp"

HART_TEST ("Utils - makeRandomSeeds() - Makes correct number of seeds")
{
    std::vector<uint_fast32_t> seedsA = hart::makeRandomSeeds<uint_fast32_t> (10);
    HART_EXPECT_EQ (seedsA.size(), 10);
    
    std::vector<uint_fast32_t> seedsB = hart::makeRandomSeeds<uint_fast32_t> (0);
    HART_EXPECT_TRUE (seedsB.empty());
}

HART_TEST ("Utils - makeRandomSeeds() - Produces unique seed values")
{
    std::vector<uint_fast32_t> seeds = hart::makeRandomSeeds<uint_fast32_t> (1000);
    HART_EXPECT_EQ (seeds.size(), 1000);

    const std::unordered_set<uint_fast32_t> uniqueSeeds (seeds.begin(), seeds.end());
    HART_EXPECT_GT (uniqueSeeds.size(), 1);
}

HART_TEST ("Utils - makeRandomSeeds() - Identical base seeds yield identical seed sequences")
{
    std::vector<uint_fast32_t> seedsA = hart::makeRandomSeeds<uint_fast32_t> (100, 42);
    std::vector<uint_fast32_t> seedsB = hart::makeRandomSeeds<uint_fast32_t> (100, 42);

    for (size_t i = 0; i < 100; ++i)
        HART_EXPECT_EQ (seedsA[i], seedsB[i]);
}

HART_TEST ("Utils - makeRandomSeeds() - Different base seeds yields different seeds sets")
{
    std::vector<uint_fast32_t> seedsA = hart::makeRandomSeeds<uint_fast32_t> (100, 42);
    std::vector<uint_fast32_t> seedsB = hart::makeRandomSeeds<uint_fast32_t> (100, 67);

    // Not using "EXPECT_NE" here because "!=" operator was removed from vectors in C++20
    HART_EXPECT_FALSE (seedsA == seedsB);
}

HART_TEST ("Utils - secondsToFrames() - Normal use cases")
{
    using hart::secondsToFrames;
    HART_EXPECT_EQ (secondsToFrames (1_s, 44100_Hz), 44100);
    HART_EXPECT_EQ (secondsToFrames (1_s, 48000_Hz), 48000);
    HART_EXPECT_EQ (secondsToFrames (1_s, 196000_Hz), 196000);
    HART_EXPECT_EQ (secondsToFrames (100_ms, 44100_Hz), 4410);
    HART_EXPECT_EQ (secondsToFrames (1_ms, 44100_Hz), 44);
    HART_EXPECT_EQ (secondsToFrames (100_us, 44100_Hz), 4);
    HART_EXPECT_EQ (secondsToFrames (100_us, 48000_Hz), 5);
    HART_EXPECT_EQ (secondsToFrames (10_us, 44100_Hz), 0);
    HART_EXPECT_EQ (secondsToFrames (10_us, 88200_Hz), 1);
    HART_EXPECT_EQ (secondsToFrames (1_us, 88200_Hz), 0);
    HART_EXPECT_EQ (secondsToFrames (0_s, 44100_Hz), 0);
    HART_EXPECT_EQ (secondsToFrames (0_s, 48000_Hz), 0);
}

HART_TEST ("Utils - framesToSeconds() - Normal use cases")
{
    using hart::framesToSeconds;

    HART_EXPECT_FLOAT_EQ (framesToSeconds (44100, 44100_Hz), 1_s, 1e-16);
    HART_EXPECT_FLOAT_EQ (framesToSeconds (4410, 44100_Hz), 100_ms, 1e-16);
    HART_EXPECT_FLOAT_EQ (framesToSeconds (441, 44100_Hz), 10_ms, 1e-16);
    HART_EXPECT_FLOAT_EQ (framesToSeconds (44, 44100_Hz), 1_ms, 10_us);
    HART_EXPECT_FLOAT_EQ (framesToSeconds (4, 44100_Hz), 100_us, 10_us);
    HART_EXPECT_FLOAT_EQ (framesToSeconds (24000, 48000_Hz), 500_ms, 1e-16);
    HART_EXPECT_FLOAT_EQ (framesToSeconds (196, 196_kHz), 1_ms, 1e-16);
    HART_EXPECT_FLOAT_EQ (framesToSeconds (0, 44100_Hz), 0_s, 1e-16);
    HART_EXPECT_FLOAT_EQ (framesToSeconds (0, 48000_Hz), 0_s, 1e-16);
}

HART_TEST ("Utils - framesToSeconds() vs secondsToFrames() round-trips")
{
    using hart::framesToSeconds;
    using hart::secondsToFrames;

    for (const double numSeconds : { 0_s, 500_us, 3.5_ms, 100_ms, 1_s, 123.456_s, 654321_s})
        HART_EXPECT_FLOAT_EQ (framesToSeconds (secondsToFrames (numSeconds)), numSeconds, 10_us);
    
    for (const size_t numFrames : { 0, 1, 5, 100, 4000, 100000, 12345678, 999999999 })
        HART_EXPECT_EQ (secondsToFrames (framesToSeconds (numFrames)), numFrames);
}

HART_TEST ("Utils - nextPowerOfTwoDuration() - Output value is no less than targer duration")
{
    for (double targetDurationSeconds = 0.1_us; targetDurationSeconds < 100000_s; targetDurationSeconds *= 1.9)
    {
        const double suggestedDurationSeconds = hart::nextPowerOfTwoDuration (targetDurationSeconds);
        HART_EXPECT_GE (suggestedDurationSeconds, targetDurationSeconds);
    }
}

HART_TEST ("Utils - nextPowerOfTwoDuration() - Output duration is power of 2 in frames")
{
    for (double targetDurationSeconds = 0.1_us; targetDurationSeconds < 100000_s; targetDurationSeconds *= 1.9)
    {
        const double suggestedDurationSeconds = hart::nextPowerOfTwoDuration (targetDurationSeconds);
        const size_t suggestedDurationFrames = hart::secondsToFrames (suggestedDurationSeconds);

        HART_EXPECT_TRUE (hart::isPowerOfTwo (suggestedDurationFrames));
    }
}

HART_TEST ("Utils - nextPowerOfTwoDuration() - Output is at least 1 frame long")
{
    const double oneFrameDurationSeconds = hart::framesToSeconds (1);

    for (const double targetDurationSeconds : { 0.0, 1e-16, 0.1_us, 1_us, std::numeric_limits<double>::denorm_min(), std::numeric_limits<double>::min() })
    {
        const double suggestedDurationSeconds = hart::nextPowerOfTwoDuration (targetDurationSeconds);
        HART_EXPECT_GE (suggestedDurationSeconds, oneFrameDurationSeconds);
    }
}
