#pragma once

// Platform-specific headers for hassert() - Begin
#if defined (_MSC_VER)
#include <intrin.h>
#endif
#if defined (__APPLE__)
#include <sys/sysctl.h>
#include <sys/types.h>
#include <unistd.h>
#elif defined (__linux__) || defined (__FreeBSD__) || defined (__OpenBSD__) || defined (__NetBSD__)
#include <csignal>
#include <unistd.h>
#endif
// Platform-specific headers for hassert() - End

#include <cstdio>

namespace hart
{

#if defined (_WIN32)
extern "C" __declspec (dllimport) int __stdcall IsDebuggerPresent();
#endif

/// @brief A helper for hassert() statements
/// @private
inline bool isRunningUnderDebugger()
{
    #if defined (_WIN32)
        return IsDebuggerPresent() != 0;

    #elif defined (__APPLE__)
        kinfo_proc info;
        int mib[] = { CTL_KERN, KERN_PROC, KERN_PROC_PID, getpid() };
        size_t infoSize = sizeof (info);

        if (sysctl (mib, 4, &info, &infoSize, nullptr, 0) != 0)
            return false;

        return (info.kp_proc.p_flag & P_TRACED) != 0;

    #elif defined (__linux__)
        FILE* status = std::fopen ("/proc/self/status", "r");

        if (status == nullptr)
            return false;

        char line[128];

        while (std::fgets (line, sizeof (line), status) != nullptr)
        {
            int tracerPid = 0;

            if (std::sscanf (line, "TracerPid:\t%d", &tracerPid) == 1)
            {
                std::fclose (status);
                return tracerPid != 0;
            }
        }

        std::fclose (status);
        return false;

    #else
        return false;

    #endif
}

}  // namespace hart

#if defined (_MSC_VER)
    #define HART_BREAK_IN_DEBUGGER __debugbreak()
    #elif defined (__APPLE__) && defined (__clang__) && defined (__has_builtin)
        #if __has_builtin (__builtin_debugtrap)
            #define HART_BREAK_IN_DEBUGGER __builtin_debugtrap()
        #else
            #define HART_BREAK_IN_DEBUGGER __builtin_trap()
        #endif

#elif defined (__linux__) || defined (__FreeBSD__) || defined (__OpenBSD__) || defined (__NetBSD__)
    #define HART_BREAK_IN_DEBUGGER ::kill (0, SIGTRAP)

#elif defined (__GNUC__) || defined (__clang__)
    #define HART_BREAK_IN_DEBUGGER __builtin_trap()

#else
    #define HART_BREAK_IN_DEBUGGER ((void) 0)

#endif

#if defined (_MSC_VER)
    #define HART_HASSERT_BEGIN __pragma (warning (push)) __pragma (warning (disable: 4127))
    #define HART_HASSERT_END __pragma (warning (pop))
#else
    #define HART_HASSERT_BEGIN
    #define HART_HASSERT_END
#endif

/// @brief Triggers a breakpoint when a debugger is attached, throws or logs a `HartAssertException` otherwise
/// @ingroup Exceptions
#define hassertfalse HART_HASSERT_BEGIN if (hart::isRunningUnderDebugger()) { HART_BREAK_IN_DEBUGGER; } else { HART_THROW (hart::HartAssertException, "hassertfalse failed"); } HART_HASSERT_END

/// @brief Triggers a breakpoint when a debugger is attached, throws or logs a `HartAssertException` otherwise
/// @ingroup Exceptions
#define hassert(condition) HART_HASSERT_BEGIN if (! (condition)) { if (hart::isRunningUnderDebugger()) { HART_BREAK_IN_DEBUGGER; } else { HART_THROW (hart::HartAssertException, std::string ("hassert failed:") + #condition); } } HART_HASSERT_END
