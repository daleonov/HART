#pragma once

#include <iostream>
#include <stdexcept>

/// @defgroup Exceptions Exceptions
/// @brief Exceptions and error handling

namespace hart
{

/// @brief Thrown by test asserts like `HART_ASSERT_TRUE()` and `AudioTestBuilder::assertFalse()`
class TestAssertException:
    public std::runtime_error
{
    using std::runtime_error::runtime_error;
};

/// @brief Thrown when some I/O operation fails
/// @ingroup Exceptions
class IOError:
    public std::runtime_error
{
    using std::runtime_error::runtime_error;
};

/// @brief Thrown when some unexpected state is encountered
/// @ingroup Exceptions
class StateError:
    public std::runtime_error
{
    using std::runtime_error::runtime_error;
};

/// @brief Thrown when an unexpected container size is encountered
/// @ingroup Exceptions
class SizeError:
    public std::runtime_error
{
    using std::runtime_error::runtime_error;
};

/// @brief Thrown when an inappropriate value is encountered
/// @ingroup Exceptions
class ValueError:
    public std::runtime_error
{
    using std::runtime_error::runtime_error;
};

/// @brief Thrown when sample rate is mismatched
/// @ingroup Exceptions
class SampleRateError:
    public std::runtime_error
{
    using std::runtime_error::runtime_error;
};

/// @brief Thrown when a numbers of channels is mismatched
/// @ingroup Exceptions
class ChannelLayoutError:
    public std::runtime_error
{
    using std::runtime_error::runtime_error;
};

/// @brief Thrown when `hassert()` or `hassertfalse` are triggered
/// @ingroup Exceptions
class HartAssertException:
    public std::runtime_error
{
    using std::runtime_error::runtime_error;
};

/// @brief Thrown when some parameter has an unsupported value
/// @ingroup Exceptions
class UnsupportedError:
    public std::runtime_error
{
    using std::runtime_error::runtime_error;
};

/// @brief Thrown when the test runner is misconfigured
/// @details For example, if a CLI data directory argument is required, but missing
/// @ingroup Exceptions
class ConfigurationError:
    public std::runtime_error
{
    using std::runtime_error::runtime_error;
};

/// @brief Thrown when a container index is out of range
/// @ingroup Exceptions
class IndexError:
    public std::runtime_error
{
    using std::runtime_error::runtime_error;
};

/// @brief Thrown when a nullptr could be handled gracefully
/// @ingroup Exceptions
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

/// @brief Throws an exception if `HART_DO_NOT_THROW_EXCEPTIONS` is set, prints a message otherwise
/// @ingroup Exceptions
#define HART_THROW(ExceptionType, message) do { HART_THROW_IMPL(ExceptionType, message); } while (0)

/// @brief Throws an exception if `HART_DO_NOT_THROW_EXCEPTIONS` is set, prints a message and returns a specified value otherwise
/// @ingroup Exceptions
#define HART_THROW_OR_RETURN(ExceptionType, message, returnValue) { HART_THROW_IMPL (ExceptionType, message); return returnValue; }

/// @brief Throws an exception if `HART_DO_NOT_THROW_EXCEPTIONS` is set, prints a message and returns otherwise
/// @ingroup Exceptions
#define HART_THROW_OR_RETURN_VOID(ExceptionType, message) { HART_THROW_IMPL (ExceptionType, message); return; }

/// @brief Throws an exception if `HART_DO_NOT_THROW_EXCEPTIONS` is set, prints a message and jumps to the next iteration (via `continue`) otherwise
/// @ingroup Exceptions
#define HART_THROW_OR_CONTINUE(ExceptionType, message) { HART_THROW_IMPL (ExceptionType, message); continue; }

#else

#define HART_THROW_IMPL(ExceptionType, message) throw ExceptionType (std::string (message) +  ", file: " __FILE__ ", line: " HART_LINE_STRING)

/// @brief Throws an exception if `HART_DO_NOT_THROW_EXCEPTIONS` is set, prints a message otherwise
/// @ingroup Exceptions
#define HART_THROW(ExceptionType, message) do { HART_THROW_IMPL(ExceptionType, message); } while (0)

/// @brief Throws an exception if `HART_DO_NOT_THROW_EXCEPTIONS` is set, prints a message and returns a specified value otherwise
/// @ingroup Exceptions
#define HART_THROW_OR_RETURN(ExceptionType, message, returnValue) HART_THROW (ExceptionType, message)

/// @brief Throws an exception if `HART_DO_NOT_THROW_EXCEPTIONS` is set, prints a message and returns otherwise
/// @ingroup Exceptions
#define HART_THROW_OR_RETURN_VOID(ExceptionType, message) HART_THROW (ExceptionType, message)

/// @brief Throws an exception if `HART_DO_NOT_THROW_EXCEPTIONS` is set, prints a message and jumps to the next iteration (via `continue`) otherwise
/// @ingroup Exceptions
#define HART_THROW_OR_CONTINUE(ExceptionType, message) HART_THROW (ExceptionType, message)
#endif  // HART_DO_NOT_THROW_EXCEPTIONS

// TODO: Make hassert() and hassertfalse not throw if HART_DO_NOT_THROW_EXCEPTIONS is set

/// @brief Triggers a `HartAssertException`
/// @ingroup Exceptions
#define hassertfalse HART_THROW (hart::HartAssertException, "hassertfalse failed")

/// @brief Triggers a `HartAssertException` if the `condition` is `false`
/// @ingroup Exceptions
#define hassert(condition) if (! (condition)) { HART_THROW (hart::HartAssertException, std::string ("hassert failed:") + #condition); }

/// @brief Prints a warning message
/// @ingroup Exceptions
#define HART_WARNING(message) std::cout << "Warning: " << message << ", file: " << __FILE__ << ", line: " << __LINE__ << std::endl

}  // namespace hart
