#pragma once

#include <iostream>
#include <string>
#include <unordered_set>
#include <vector>

#include "hart_cliconfig.hpp"
#include "hart_exceptions.hpp"
#include "hart_expectation_failure_messages.hpp"

namespace hart
{

struct TestInfo
{
    std::string name;
    std::string tags;
    void (*func)();
};

class TestRegistry {
public:
    static TestRegistry& getInstance()
    {
        static TestRegistry reg;
        return reg;
    }

    void add (const std::string& name, const std::string& tags, void(*func)())
    {
        const auto insertResult = registeredTestNames.insert (name);

        if (const bool wasInserted = insertResult.second != true)
            HART_THROW_OR_RETURN_VOID (hart::ValueError, std::string ("Duplicate test case name found: ") + name);

        tests.emplace_back (TestInfo {name, tags, func});
    }

    int runAll (int argc, char** argv)
    {
        const int parseCommandLineArgsResult = hart::CLIConfig::get().parseCommandLineArgs (argc, argv);
    
        if (parseCommandLineArgsResult != 0)
            return parseCommandLineArgsResult;

        size_t numPassed = 0;
        size_t numFailed = 0;

        // TODO: Optional shuffle before running

        for (const TestInfo& test : tests)
        {
            std::cout << "[  ...   ] Running " << test.name;
            bool assertionFailed = false;
            std::string assertionFailMessage;
            ExpectationFailureMessages::clear();

            try
            {
                test.func();
            }
            catch (const hart::TestAssertException& e)
            {
                assertionFailMessage = e.what();
                assertionFailed = true;
            }

            // TODO: Output test durations

            std::cout << '\r';
            const bool expectationsFailed = ExpectationFailureMessages::get().size() > 0;

            if (assertionFailed || expectationsFailed)
            {
                std::cout << "[  </3   ] " << test.name << " - failed" << std::endl;

                if (assertionFailed)
                    std::cout << "           " << assertionFailMessage << std::endl;

                for (const std::string& expectationFailureMessage : ExpectationFailureMessages::get())
                    std::cout << "           "  << expectationFailureMessage << std::endl;

                ++numFailed;
            }
            else
            {
                std::cout << "[   <3   ] " << test.name << " - passed" << std::endl;
                ++numPassed;
            }

        }

        std::cout << std::endl;
        std::cout << "[ PASSED ] " << numPassed << '/' << tests.size() << std::endl;

        if (numFailed > 0)
            std::cout << "[ FAILED ] " << numFailed << '/' << tests.size() << std::endl;

        return (int) (numFailed != 0);
    }

private:
    TestRegistry() = default;  // Private ctor for singleton
    std::vector<TestInfo> tests;
    std::unordered_set<std::string> registeredTestNames;
};

}  // namespace hart
