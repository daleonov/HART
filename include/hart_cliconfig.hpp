#pragma once

#include <cstdint>
#include "CLI11.hpp"

namespace hart
{

struct CLIConfig
{
public:
    static CLIConfig& get()
    {
        static CLIConfig cfg;
        return cfg;
    }

    int parseCommandLineArgs (int argc, char** argv)
    {
        CLI::App app { "HART" };
        app.add_option ("--data-root-path,-p", m_dataRootPath, "Data root path");
        app.add_option ("--tags", m_tags, "Test tags");
        app.add_option ("--seed,-s", m_seed, "Random seed")->default_val (0);
        app.add_flag ("--generators", m_runGeneratorsNotTests, "Run generators instead of tests");
        app.add_flag ("--shuffle", m_shuffle, "Shuffle test order");
        CLI11_PARSE (app, argc, argv);
        return 0;
    }

    std::string getDataRootPath() { return m_dataRootPath; }
    uint_fast32_t getRandomSeed() { return m_seed; }

private:

    std::string m_dataRootPath = ".";
    std::string m_tags = "";
    uint_fast32_t m_seed = 0;
    bool m_runGeneratorsNotTests = false;
    bool m_shuffle = false;

    CLIConfig() = default;
};

}  // namespace hart
