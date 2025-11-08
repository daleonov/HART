#pragma once

#include <string>
#include <vector>

namespace hart
{

// TODO: Don't expose the vector directly, implement proper interface
class ExpectationFailureMessages {
public:
    static auto& get()
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
