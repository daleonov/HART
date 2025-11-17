#pragma once

#include <iostream>
#include <string>
#include <unordered_set>
#include <vector>

#include "hart_ascii_art.hpp"
#include "hart_cliconfig.hpp"
#include "hart_exceptions.hpp"
#include "hart_expectation_failure_messages.hpp"

namespace hart
{

enum class TaskCategory
{
    test,
    generate
};

class TestRegistry {
public:

    static TestRegistry& getInstance()
    {
        static TestRegistry reg;
        return reg;
    }

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

    int runAll()
    {
        // TODO: Optional shuffle before running
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

        // TODO: Output test durations

        std::cout << '\r';
        const bool expectationsFailed = ExpectationFailureMessages::get().size() > 0;

        if (assertionFailed || expectationsFailed)
        {
            constexpr char separator[] = "-------------------------------------------";
            std::cout << "[  </3   ] " << task.name << " - failed" << std::endl;

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
            std::cout << "[   <3   ] " << task.name << " - passed" << std::endl;
            ++tasksPassed;
        }
    }
};

}  // namespace hart
