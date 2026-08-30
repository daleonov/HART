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
    const double sampleRateHz = hart::CLIConfig::getInstance().getDefaultSampleRateHz();

    for (double targetDurationSeconds = 0.1_us; targetDurationSeconds < 100000_s; targetDurationSeconds *= 1.9)
    {
        const double suggestedDurationSeconds = hart::nextPowerOfTwoDuration (targetDurationSeconds);
        const size_t suggestedDurationFrames = hart::roundToSizeT (suggestedDurationSeconds * sampleRateHz);

        HART_EXPECT_TRUE (hart::isPowerOfTwo (suggestedDurationFrames));
    }
}

HART_TEST ("Utils - nextPowerOfTwoDuration() - Output is at least 1 frame long")
{
    const double sampleRateHz = hart::CLIConfig::getInstance().getDefaultSampleRateHz();
    const double oneFrameDurationSeconds = 1.0 / sampleRateHz;

    for (const double targetDurationSeconds : { 0.0, 1e-16, 0.1_us, 1_us, std::numeric_limits<double>::denorm_min(), std::numeric_limits<double>::min() })
    {
        const double suggestedDurationSeconds = hart::nextPowerOfTwoDuration (targetDurationSeconds);
        HART_EXPECT_GE (suggestedDurationSeconds, oneFrameDurationSeconds);
    }
}
