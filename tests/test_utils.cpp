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
