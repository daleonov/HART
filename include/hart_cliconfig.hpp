#pragma once

#include <cstdint>
#include "dependencies/CLI11/CLI11.hpp"

namespace hart
{

/// @brief Holds values set by the user via CLI interface
/// @details It's mostly intended for the internal use, but you may access it in your own test cases as well
/// @ingroup TestRunner
struct CLIConfig
{
public:
    /// @brief Get the singleton instance
    static CLIConfig& getInstance()
    {
        static CLIConfig cfg;
        return cfg;
    }

    /// @brief Inits the CLI arguments
    void initCommandLineArgs()
    {
        app.add_option ("--data-root-path,-d", m_dataRootPath, "Data root path");
        app.add_option ("--tags,-t", m_tags, "Test tags");
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

        app.add_option (
            "--cents-decimals",
            m_centsDecimals,
            "Number of displayed decimal places for values in cents in test output"
            )->default_val (0);

        app.add_option (
            "--block-size",
            m_defaultBlockSizeFrames,
            "Default max block size in frames (samples). All tests will run with this block size, unless other value is explicitly set via withBlockSize()."
            )->default_val (1024);

        app.add_option (
            "--input-channels",
            m_defaultNumInputChannels,
            "Default number of input channels. All tests will run with this number of output channels, unless other value is explicitly selected via withOutputChannels() or a similar modifier."
            )->default_val (1);

        app.add_option (
            "--output-channels",
            m_defaultNumOutputChannels,
            "Default number of output channels. All tests will run with this number of input channels, unless other value is explicitly selected via withInputChannels() or a similar modifier."
            )->default_val (1);

        app.add_option (
            "--sample-rate",
            m_defaultSampleRateHz,
            "Default sample rate in Hz. All tests will run with this sample rate, unless other value is explicitly stated via withSampleRate()."
            )->default_val (44100.0);

        app.add_option (
            "--render-duration",
            m_defaultRenderDurationSeconds,
            "Default render duration in seconds. All tests will render this amount of audio (excluding optional warm-up time), unless explicitly overridden with withDuration()."
            )->default_val (0.1);

        app.add_flag ("--run-generators,-g", m_runGeneratorsNotTests, "Run generators instead of tests");
        app.add_flag ("--shuffle", m_shuffle, "Shuffle task order. Obeys --seed value.");
    }

    CLI::App& getCLIApp() { return app; }

    /// @brief Get data root path set by a "`--data-root-path`,`-d`" argument
    std::string getDataRootPath() const { return m_dataRootPath; }

    /// @brief Gets random seed set by a "`--seed`/`-s`" argument
    /// @details You can use it in your test cases to keep your own random-ness dependent on the global random seed
    uint_fast32_t getRandomSeed() const { return m_seed; }

    /// @see linPrecision
    int getLinDecimals() const { return m_linDecimals; }

    /// @see dbPrecision
    int getDbDecimals() const { return m_dbDecimals; }

    /// @see secPrecision
    int getSecDecimals() const { return m_secDecimals; }

    /// @see hzPrecision
    int getHzDecimals() const { return m_hzDecimals; }

    /// @see radPrecision
    int getRadDecimals() const { return m_radDecimals; }

    /// @see centsPrecision
    int getCentsDecimals() const { return m_centsDecimals; }

    bool shouldRunGenerators() const { return m_runGeneratorsNotTests; }
    bool shouldShuffleTasks() const { return m_shuffle; }
    std::string getTags() const { return m_tags; }

    size_t getGefaultBlockSizeFrames() const { return m_defaultBlockSizeFrames; }
    size_t getDefaultNumInputChannels() const { return m_defaultNumInputChannels; }
    size_t getDefaultNumOutputChannels() const { return m_defaultNumOutputChannels; }
    double getDefaultSampleRateHz() const { return m_defaultSampleRateHz; }
    double getDefaultRenderDurationSeconds() const { return m_defaultRenderDurationSeconds; }
    size_t getDefaultRenderDurationFrames() const { return static_cast<size_t> (m_defaultRenderDurationSeconds * m_defaultSampleRateHz + 0.5); }

private:
    CLI::App app { "HART" };

    std::string m_dataRootPath = ".";
    std::string m_tags = "";
    uint_fast32_t m_seed = 0;
    bool m_runGeneratorsNotTests = false;
    bool m_shuffle = false;

    int m_linDecimals = 0;
    int m_dbDecimals = 0;
    int m_secDecimals = 0;
    int m_hzDecimals = 0;
    int m_radDecimals = 0;
    int m_centsDecimals = 0;

    size_t m_defaultBlockSizeFrames = 1024;
    size_t m_defaultNumInputChannels = 1;
    size_t m_defaultNumOutputChannels = 1;
    double m_defaultSampleRateHz = 44100.0;
    double m_defaultRenderDurationSeconds = 0.1;

    CLIConfig() = default;
};

}  // namespace hart
