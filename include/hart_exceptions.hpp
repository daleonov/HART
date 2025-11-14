#pragma once

#include <iostream>
#include <stdexcept>

namespace hart
{

class TestAssertException:
    public std::runtime_error
{
    using std::runtime_error::runtime_error;
};

class IOError:
    public std::runtime_error
{
    using std::runtime_error::runtime_error;
};

class StateError:
    public std::runtime_error
{
    using std::runtime_error::runtime_error;
};

class SizeError:
    public std::runtime_error
{
    using std::runtime_error::runtime_error;
};

class ValueError:
    public std::runtime_error
{
    using std::runtime_error::runtime_error;
};

class SampleRateError:
    public std::runtime_error
{
    using std::runtime_error::runtime_error;
};

class ChannelLayoutError:
    public std::runtime_error
{
    using std::runtime_error::runtime_error;
};

class HartAssertException:
    public std::runtime_error
{
    using std::runtime_error::runtime_error;
};

class UnsupportedError:
    public std::runtime_error
{
    using std::runtime_error::runtime_error;
};

#ifndef HART_STRINGIFY
#define HART_STRINGIFY(x) HART_STRINGIFY2(x)
#define HART_STRINGIFY2(x) #x
#endif  // HART_STRINGIFY

#define HART_LINE_STRING HART_STRINGIFY(__LINE__)

#if HART_DO_NOT_THROW_EXCEPTIONS
#define HART_THROW(ExceptionType, message) do { std::cout << #ExceptionType << " triggered: \"" << message << "\", file: " << __FILE__ << ", line: " << __LINE__ << std::endl } while (0)
#else
#define HART_THROW(ExceptionType, message) do { throw ExceptionType (std::string (message) +  ", file: " __FILE__ ", line: " HART_LINE_STRING); } while (0)
#endif  // HART_DO_NOT_THROW_EXCEPTIONS

#define HART_THROW_OR_RETURN(ExceptionType, message, returnValue) do { HART_THROW (ExceptionType, message); return returnValue; } while (0)
#define HART_THROW_OR_RETURN_VOID(ExceptionType, message) do { HART_THROW (ExceptionType, message); return; } while(0)

#define hassertfalse HART_THROW (hart::HartAssertException, "hassertfalse failed")
#define hassert(condition) if (! (condition)) { HART_THROW (hart::HartAssertException, std::string ("hassert failed:") + #condition); }

#define HART_WARNING(message) std::cout << "Warning: " << message << ", file: " << __FILE__ << ", line: " << __LINE__ << std::endl

}  // namespace hart
