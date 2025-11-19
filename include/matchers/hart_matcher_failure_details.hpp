#pragma once

#include <string>

namespace hart
{

/// @brief Details about matcher failure
/// @see Matcher::getFailureDetails()
/// @ingroup Matchers
struct MatcherFailureDetails
{
    size_t frame;  ///< Index of frame at which the match has failed
    size_t channel;  ///< Index of channel at which the failure was detected
    std::string description;  ///< Readable description of why the match has failed.
};

}  // namespace hart
