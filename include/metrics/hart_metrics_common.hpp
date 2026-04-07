#pragma once

namespace hart
{

/// @brief A helper to determine the return type of a reducer function
/// @ingroup Metrics
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
template <typename Reducer, typename Iterator>
using ReducerResultType =
    typename ReducerResult<Reducer, Iterator>::type;

/// @brief A helper to get an iterable of channel indices to process
/// @ingroup Metrics
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

}  // namespace hart
