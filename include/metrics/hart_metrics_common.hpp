#pragma once

#include <algorithm>  // min()
#include <utility>  // pair
#include <vector>

#include "hart_audio_buffer.hpp"

namespace hart
{

/// @defgroup Metrics Metrics
/// @brief Common audio-related metrics

/// @brief Helpers to generate common default channel subsets
struct ChannelSubsets
{
    static std::vector<size_t> allChannels (size_t numChannels)
    {
        std::vector<size_t> channelIndices (numChannels);

        for (size_t i = 0; i < channelIndices.size(); ++i)
            channelIndices[i] = i;
        
        return channelIndices;
    }

    static std::vector<std::pair<size_t, size_t>> upperTriangleChannelPairs (size_t numChannels)
    {
        std::vector<std::pair<size_t, size_t>> channelPairIndices;
        channelPairIndices.reserve (numChannels * (numChannels - 1) / 2);

        for (size_t channelA = 0; channelA < numChannels; ++channelA)
            for (size_t channelB = channelA + 1; channelB < numChannels; ++channelB)
                channelPairIndices.emplace_back (channelA, channelB);

        return channelPairIndices;
    }

    static std::vector<std::pair<size_t, size_t>> diagonalChannelPairs (size_t numChannels)
    {
        std::vector<std::pair<size_t, size_t>> channelPairIndices;
        channelPairIndices.reserve (numChannels);

        for (size_t channel = 0; channel < numChannels; ++channel)
            channelPairIndices.emplace_back (channel, channel);

        return channelPairIndices;
    }
};

/// @brief A helper to determine the return type of a reducer function
/// @ingroup Metrics
/// @private
template <typename Reducer, typename Iterator>
struct ReducerResult
{
    typedef decltype (
        std::declval<Reducer>()(
            std::declval<Iterator>(),
            std::declval<Iterator>()
            )
        )
    type;
};

/// @brief An alias to get the return type of a reducer function in a less verbose post-C++11 manner
/// @ingroup Metrics
/// @private
template <typename Reducer, typename Iterator>
using ReducerResultType =
    typename ReducerResult<Reducer, Iterator>::type;

// TODO: Replace this method with std::vector generator(s)
/// @brief A helper to get an iterable of channel indices to process
/// @ingroup Metrics
/// @private
template <typename SampleType>
std::vector<size_t> getChannelIndicesToProcess (const AudioBuffer<SampleType>& buffer, std::initializer_list<size_t> channels)
{
    if (channels.size() != 0)
        return std::vector<size_t> (channels.begin(), channels.end());

    std::vector<size_t> indices (buffer.getNumChannels());

    for (size_t i = 0; i < indices.size(); ++i)
        indices[i] = i;

    return indices;
}

// TODO: Replace this method with std::vector generator(s)
/// @brief A helper to get an iterable of channel indices to process for a pair of buffers
/// @details If buffers' channel count is mismatched, returns the shortest subset
/// @ingroup Metrics
/// @private
template <typename SampleType>
std::vector<size_t> getChannelIndicesToProcess (const AudioBuffer<SampleType>& bufferA, const AudioBuffer<SampleType>& bufferB, std::initializer_list<size_t> channels)
{
    if (bufferA.getNumChannels() > bufferB.getNumChannels())
        return getChannelIndicesToProcess (bufferB, channels);

    return getChannelIndicesToProcess (bufferA, channels);
}

/// @brief Describes how to look for best cross-correlation
enum CorrelationSearchMode
{
    bestSignedCorrelation,
    bestAbsoluteCorrelation
};

}  // namespace hart
