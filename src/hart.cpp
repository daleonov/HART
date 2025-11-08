#include <vector>
#include "hart.hpp"

namespace hart
{

thread_local std::vector<std::string> expectationFailureMessages;

}  // namespace hart
