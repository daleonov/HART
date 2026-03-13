#pragma once

#include <string>
#include <vector>

namespace hart
{

// TODO: Don't expose the vector directly, implement proper interface

/// @brief Holds failure messages to be displayed by the test runner
/// @details For internal use only
/// @private
/// @ingroup DataStructures
class ExpectationFailureMessages {
public:
    static std::vector<std::string>& get()
    {
        thread_local std::vector<std::string> messages;
        return messages;
    }

    static void clear()
    {
        get().clear();
    }

private:
    ExpectationFailureMessages() = default;
};

} // namespace hart
