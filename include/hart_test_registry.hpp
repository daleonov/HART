#pragma once

#include <exception>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

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
        tests.emplace_back (TestInfo {name, tags, func});
    }

    int runAll()
    {
        size_t numPassed = 0;
        size_t numFailed = 0;

        for (const TestInfo& test : tests)
        {
            std::cout << "[  ...   ] Running " << test.name;
            bool passed = true;
            std::string failMessage;

            try
            {
                test.func();
            }
            catch (const std::exception& e)
            {
                failMessage = e.what();
                passed = false;
            }

            std::cout << '\r';
            if (passed)
            {
                std::cout << "[   <3   ] " << test.name << " passed" << std::endl;
                ++numPassed;
            }
            else
            {
                std::cout << "[  </3   ] " << test.name << " failed" << std::endl;
                std::cout << failMessage << std::endl;
                ++numFailed;
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
};

}  // namespace hart
