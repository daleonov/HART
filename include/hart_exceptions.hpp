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

class ConfigurationError:
    public std::runtime_error
{
    using std::runtime_error::runtime_error;
};

class IndexError:
    public std::runtime_error
{
    using std::runtime_error::runtime_error;
};

class NullPointerError:
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
#define HART_THROW_IMPL(ExceptionType, message) std::cout << #ExceptionType << " triggered: \"" << message << "\", file: " << __FILE__ << ", line: " << __LINE__ << std::endl
#define HART_THROW(ExceptionType, message) do { HART_THROW_IMPL(ExceptionType, message); } while (0)
#define HART_THROW_OR_RETURN(ExceptionType, message, returnValue) { HART_THROW_IMPL (ExceptionType, message); return returnValue; }
#define HART_THROW_OR_RETURN_VOID(ExceptionType, message) { HART_THROW_IMPL (ExceptionType, message); return; }
#define HART_THROW_OR_CONTINUE(ExceptionType, message) { HART_THROW_IMPL (ExceptionType, message); continue; }
#else
#define HART_THROW_IMPL(ExceptionType, message) throw ExceptionType (std::string (message) +  ", file: " __FILE__ ", line: " HART_LINE_STRING)
#define HART_THROW(ExceptionType, message) do { HART_THROW_IMPL(ExceptionType, message); } while (0)
#define HART_THROW_OR_RETURN(ExceptionType, message, returnValue) HART_THROW (ExceptionType, message)
#define HART_THROW_OR_RETURN_VOID(ExceptionType, message) HART_THROW (ExceptionType, message)
#define HART_THROW_OR_CONTINUE(ExceptionType, message) HART_THROW (ExceptionType, message)
#endif  // HART_DO_NOT_THROW_EXCEPTIONS

#define hassertfalse HART_THROW (hart::HartAssertException, "hassertfalse failed")
#define hassert(condition) if (! (condition)) { HART_THROW (hart::HartAssertException, std::string ("hassert failed:") + #condition); }

#define HART_WARNING(message) std::cout << "Warning: " << message << ", file: " << __FILE__ << ", line: " << __LINE__ << std::endl

}  // namespace hart
