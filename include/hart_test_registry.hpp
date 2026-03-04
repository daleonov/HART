#pragma once

#include <algorithm>  // shuffle()
#include <chrono>
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>
#include <string>
#include <unordered_set>
#include <vector>

#include "hart_ascii_art.hpp"
#include "hart_cliconfig.hpp"
#include "hart_exceptions.hpp"
#include "hart_expectation_failure_messages.hpp"

namespace hart
{

/// @brief Determines whether the task is a test or a generator
/// @private
enum class TaskCategory
{
    test,
    generate
};

/// @brief Runs the test cases
/// @details For internal use by HART. You're not supposed to interact with it directly, only through the macros
/// such as @ref HART_RUN_ALL_TESTS(), @ref HART_TEST(), @ref HART_TEST_WITH_TAGS(), @ref HART_GENERATE(), @ref HART_GENERATE_WITH_TAGS()
/// @ingroup TestRunner
class TestRegistry {
public:

    /// @brief Gets the singleton instance
    static TestRegistry& getInstance()
    {
        static TestRegistry reg;
        return reg;
    }

    /// @brief Adds a task (test or generator)
    /// @details Gets called when a test case is declared with a macro like @ref HART_TEST()
    /// @private
    void add (const std::string& name, const std::string& tags, TaskCategory testCategory, void (*func)())
    {
        std::unordered_set<std::string>& registeredNamesContainer =
            testCategory == TaskCategory::test
                ? registeredTestNames
                : registeredGeneratorNames;

        const auto insertResult = registeredNamesContainer.insert (name);
        const bool isDuplicate = ! insertResult.second;

        if (isDuplicate)
            HART_THROW_OR_RETURN_VOID (hart::ValueError, std::string ("Duplicate test case name found: ") + name);

        std::vector<TaskInfo>& tasks =
            testCategory == TaskCategory::test
                ? tests
                : generators;

        tasks.emplace_back (TaskInfo {name, tags, func});
    }

    /// @brief Runs all tests or generators
    int runAll()
    {
        // TODO: Make data root dir if set, but doesn't exist
        // TODO: Add support for runnings tasks in a thread pool

        std::cout << hartAsciiArt << std::endl;

        std::vector<TaskInfo>& tasks =
            CLIConfig::getInstance().shouldRunGenerators()
                ? generators
                : tests;

        if (tasks.size() == 0)
        {
            std::cout << "Nothing to run!" << std::endl;
            return 0;
        }

        if (CLIConfig::getInstance().shouldShuffleTasks())
            shuffleTasks (tasks);

        for (const TaskInfo& task : tasks)
            runTask (task);

        std::cout << std::endl;
        std::cout << "[ PASSED ] " << tasksPassed << '/' << tasks.size() << std::endl;

        if (tasksFailed > 0)
            std::cout << "[ FAILED ] " << tasksFailed << '/' << tasks.size() << std::endl;

        const char* resultAsciiArt = tasksFailed > 0 ? failAsciiArt : passAsciiArt;
        std::cout << std::endl << resultAsciiArt << std::endl;
        return (int) (tasksFailed != 0);
    }

private:
    struct TaskInfo
    {
        std::string name;
        std::string tags;
        void (*func)();
    };

    TestRegistry() = default;  // Private ctor for singleton
    std::vector<TaskInfo> tests;
    std::vector<TaskInfo> generators;
    std::unordered_set<std::string> registeredTestNames;
    std::unordered_set<std::string> registeredGeneratorNames;

    size_t tasksPassed = 0;
    size_t tasksFailed = 0;

    void runTask (const TaskInfo& task)
    {
        std::cout << "[  ...   ] Running " << task.name;
        bool assertionFailed = false;
        std::string assertionFailMessage;
        ExpectationFailureMessages::clear();

        const auto timestampStart = std::chrono::high_resolution_clock::now();

        try
        {
            task.func();
        }
        catch (const hart::TestAssertException& e)
        {
            assertionFailMessage = e.what();
            assertionFailed = true;
        }
        catch (const hart::ConfigurationError& e)
        {
            assertionFailMessage = e.what();
            assertionFailed = true;
        }

        const auto timestampFinish = std::chrono::high_resolution_clock::now();
        const auto testDuration = timestampFinish - timestampStart;

        // TODO: Output test durations

        std::cout << '\r';
        const bool expectationsFailed = ExpectationFailureMessages::get().size() > 0;
        const std::string testDurationLabel = formatDuration (testDuration);

        if (assertionFailed || expectationsFailed)
        {
            constexpr char separator[] = "-------------------------------------------";
            std::cout << "[  </3   ] " << testDurationLabel << task.name << " - failed" << std::endl;

            if (assertionFailed)
            {
                std::cout << separator << std::endl << assertionFailMessage << std::endl;
            }

            for (const std::string& expectationFailureMessage : ExpectationFailureMessages::get())
            {
                std::cout << separator << std::endl << expectationFailureMessage << std::endl;
            }

            std::cout << separator << std::endl;
            ++tasksFailed;
        }
        else
        {
            std::cout << "[   <3   ] " << testDurationLabel << task.name << " - passed" << std::endl;
            ++tasksPassed;
        }
    }

    static void shuffleTasks (std::vector<TaskInfo>& tasks)
    {
        std::mt19937 rng (CLIConfig::getInstance().getRandomSeed());
        std::shuffle (tasks.begin(), tasks.end(), rng);
    }

    static std::string formatDuration (std::chrono::high_resolution_clock::duration duration)
    {
        using std::chrono::duration_cast;
        using std::chrono::microseconds;
        const long long int durationUs = duration_cast<microseconds> (duration).count();
        constexpr int targetWidth = 7;
        std::ostringstream oss;
        oss << '[';

        if (durationUs >= 1'000'000)
        {
            const double durationSeconds = durationUs / 1'000'000.0;
            oss << std::setw (targetWidth - 2) << std::right
                << std::fixed << std::setprecision (2)
                << durationSeconds << " s] ";
        }
        else if (durationUs >= 1000)
        {
            const long long int durationMs = durationUs / 1000;
            oss << std::setw (targetWidth - 3) << std::right
                << durationMs << " ms] ";
        }
        else
        {
            oss << std::setw (targetWidth - 3) << std::right
                << durationUs << " us] ";
        }

        return oss.str();
    }
};

}  // namespace hart
