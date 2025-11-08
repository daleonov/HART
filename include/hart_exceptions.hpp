#pragma once

#include <stdexcept>

namespace hart
{

class TestAssertException:
    public std::runtime_error
{
    using std::runtime_error::runtime_error;
};

}  // namespace hart
