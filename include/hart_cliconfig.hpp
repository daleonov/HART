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

        app.add_option (
            "--linear-value-display-decimals",
            m_linearValueDisplayDecimals,
            "Number of displayed decimal places for samples' linear values in test output"
            )->default_val (6);

        app.add_option (
            "--db-value-display-decimals",
            m_dbValueDisplayDecimals,
            "Number of displayed decimal places for values in decidels in test output"
            )->default_val (1);

        app.add_option (
            "--seconds-value-display-decimals",
            m_secondsValueDisplayDecimals,
            "Number of displayed decimal places for values in seconds in test output"
            )->default_val (3);

        app.add_flag ("--generators", m_runGeneratorsNotTests, "Run generators instead of tests");
        app.add_flag ("--shuffle", m_shuffle, "Shuffle test order");
        CLI11_PARSE (app, argc, argv);
        return 0;
    }

    std::string getDataRootPath() { return m_dataRootPath; }
    uint_fast32_t getRandomSeed() { return m_seed; }
    int getLinearValueDisplayDecimals() { return m_linearValueDisplayDecimals; }
    int getDbValueDisplayDecimals() { return m_dbValueDisplayDecimals; }
    int getSecondsValueDisplayDecimals() { return m_secondsValueDisplayDecimals; }

private:

    std::string m_dataRootPath = ".";
    std::string m_tags = "";
    uint_fast32_t m_seed = 0;
    bool m_runGeneratorsNotTests = false;
    bool m_shuffle = false;

    int m_linearValueDisplayDecimals;
    int m_dbValueDisplayDecimals;
    int m_secondsValueDisplayDecimals;

    CLIConfig() = default;
};

}  // namespace hart
