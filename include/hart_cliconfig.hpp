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
            "--lin-decimals",
            m_linDecimals,
            "Number of displayed decimal places for samples' linear values in test output"
            )->default_val (6);

        app.add_option (
            "--db-decimals",
            m_dbDecimals,
            "Number of displayed decimal places for values in decidels in test output"
            )->default_val (1);

        app.add_option (
            "--sec-decimals",
            m_secDecimals,
            "Number of displayed decimal places for values in seconds in test output"
            )->default_val (3);

        app.add_option (
            "--hz-decimals",
            m_hzDecimals,
            "Number of displayed decimal places for values in hertz in test output"
            )->default_val (1);

        app.add_option (
            "--rad-decimals",
            m_radDecimals,
            "Number of displayed decimal places for values in radians in test output"
            )->default_val (1);

        app.add_flag ("--generators", m_runGeneratorsNotTests, "Run generators instead of tests");
        app.add_flag ("--shuffle", m_shuffle, "Shuffle test order");
        CLI11_PARSE (app, argc, argv);
        return 0;
    }

    std::string getDataRootPath() { return m_dataRootPath; }
    uint_fast32_t getRandomSeed() { return m_seed; }

    /// @see linPrecision
    int getLinDecimals() { return m_linDecimals; }

    /// @see dbPrecision
    int getDbDecimals() { return m_dbDecimals; }

    /// @see secPrecision
    int getSecDecimals() { return m_secDecimals; }

    /// @see hzPrecision
    int getHzDecimals() { return m_hzDecimals; }

    /// @see radPrecision
    int getRadDecimals() { return m_radDecimals; }

private:

    std::string m_dataRootPath = ".";
    std::string m_tags = "";
    uint_fast32_t m_seed = 0;
    bool m_runGeneratorsNotTests = false;
    bool m_shuffle = false;

    int m_linDecimals;
    int m_dbDecimals;
    int m_secDecimals;
    int m_hzDecimals;
    int m_radDecimals;

    CLIConfig() = default;
};

}  // namespace hart
