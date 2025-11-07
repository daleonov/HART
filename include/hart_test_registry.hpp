#pragma once

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

    void runAll()
    {
        for (const TestInfo& test : tests)
        {
            std::cout << "Running " << test.name << " with tags: " << test.tags << std::endl;
            test.func();
        }
    }

private:
    TestRegistry() = default;  // Private ctor for singleton
    std::vector<TestInfo> tests;
};

}  // namespace hart
