#pragma once

#include <stdexcept>

#define HART_THROW(msg) throw std::runtime_error (msg)

namespace hart
{

class TestAssertException:
    public std::runtime_error
{
    using std::runtime_error::runtime_error;
};

class ChannelMismatchException:
    public std::runtime_error
{
    using std::runtime_error::runtime_error;
};

class FileIOException:
    public std::runtime_error
{
    using std::runtime_error::runtime_error;
};

}  // namespace hart
