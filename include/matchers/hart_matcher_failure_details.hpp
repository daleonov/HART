#pragma once

#include <string>

namespace hart
{

// TODO: Some reserved const values for "all channels/frames" or "irrelevant" cases

/// @brief Details about matcher failure
/// @see Matcher::getFailureDetails()
/// @ingroup Matchers
struct MatcherFailureDetails
{
    size_t frame = 0;  ///< Index of frame at which the match has failed
    size_t channel = 0;  ///< Index of channel at which the failure was detected
    std::string description;  ///< Readable description of why the match has failed.
};

}  // namespace hart
