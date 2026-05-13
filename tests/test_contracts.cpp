#include "hart.hpp"
#include "dsp_contract_checker.hpp"
#include "envelope_contract_checker.hpp"
#include "matcher_contract_checker.hpp"
#include "metric_contract_checkers.hpp"
#include "signal_contract_checker.hpp"

HART_DECLARE_ALIASES_FOR_FLOAT;

static void verify (const std::unique_ptr<hart::DSPBase<float>>& dsp)
{
    const DSPContractChecker* dspContractChecker = dynamic_cast<DSPContractChecker*> (dsp.get());
    HART_ASSERT_NE (dspContractChecker, nullptr);
    dspContractChecker->verify();
}

static void verify (const std::unique_ptr<hart::SignalBase<float>>& signal)
{
    const SignalContractChecker* signalContractChecker = dynamic_cast<SignalContractChecker*> (signal.get());
    HART_ASSERT_NE (signalContractChecker, nullptr);
    signalContractChecker->verify();
}

HART_TEST ("DSP Contracts - Test Runner")
{
    DSPContractChecker dspContractChecker = DSPContractChecker();
    auto usedDsp = processAudioWith (std::move (dspContractChecker))
        .withLabel ("All default")
        .process();
    verify (usedDsp);

    dspContractChecker = DSPContractChecker()
        .withExpectedMaxBlockSize (123)
        .withExpectedChannelLayout (7, 11)
        .withExpectedSampleRate (45.658_kHz)
        .withExpectedRenderedDuration (347_ms);
    usedDsp = processAudioWith (std::move (dspContractChecker))
        .withLabel ("All custom")
        .withSampleRate (45.658_kHz)
        .withInputChannels (7)
        .withOutputChannels (11)
        .withBlockSize (123)
        .withDuration (347_ms)
        .process();
    verify (usedDsp);

    dspContractChecker = DSPContractChecker()
        .withExpectedMaxBlockSize (1)
        .withExpectedRenderedDuration (1_ms);
    usedDsp = processAudioWith (std::move (dspContractChecker))
        .withLabel ("Block size of 1 frame")
        .withBlockSize (1)
        .withDuration (1_ms)
        .process();
    verify (usedDsp);

    dspContractChecker = DSPContractChecker()
        .withExpectedSampleRate (44100_Hz)
        .withExpectedRenderedDuration (100_ms)
        .withExpectedMaxBlockSize (4410);
    usedDsp = processAudioWith (std::move (dspContractChecker))
        .withLabel ("Render in a single large block")
        .withSampleRate (44100_Hz)
        .withDuration (100_ms)
        .withBlockSize (4410)
        .process();
    verify (usedDsp);
}

HART_TEST ("DSP Contracts - Signal's DSP Chain")
{
    std::unique_ptr<hart::SignalBase<float>> usedSignal;

    DSPContractChecker dspContractChecker = DSPContractChecker();
    processAudioWith (GainDb (0_dB))
        .withLabel ("All default")
        .withInputSignal (Silence() >> std::move (dspContractChecker))
        .saveInputSignalTo (usedSignal)
        .process();
    verify (usedSignal->popDSP());

    dspContractChecker = DSPContractChecker()
        .withExpectedMaxBlockSize (123)
        .withExpectedChannelLayout (3, 3)
        .withExpectedSampleRate (45.658_kHz)
        .withExpectedRenderedDuration (347_ms);
    processAudioWith (GainDb (0_dB))
        .withLabel ("All custom")
        .withInputSignal (Silence() >> std::move (dspContractChecker))
        .saveInputSignalTo (usedSignal)
        .withSampleRate (45.658_kHz)
        .withInputChannels (3)
        .withOutputChannels (3)
        .withBlockSize (123)
        .withDuration (347_ms)
        .process();
    verify (usedSignal->popDSP());

    dspContractChecker = DSPContractChecker()
        .withExpectedMaxBlockSize (1)
        .withExpectedRenderedDuration (1_ms);
    processAudioWith (GainDb (0_dB))
        .withLabel ("Block size of 1 frame")
        .withInputSignal (Silence() >> std::move (dspContractChecker))
        .saveInputSignalTo (usedSignal)
        .withBlockSize (1)
        .withDuration (1_ms)
        .process();
    verify (usedSignal->popDSP());

    dspContractChecker = DSPContractChecker()
        .withExpectedSampleRate (44100_Hz)
        .withExpectedRenderedDuration (100_ms)
        .withExpectedMaxBlockSize (4410);
    processAudioWith (GainDb (0_dB))
        .withLabel ("Render in a single large block")
        .withInputSignal (Silence() >> std::move (dspContractChecker))
        .saveInputSignalTo (usedSignal)
        .withSampleRate (44100_Hz)
        .withDuration (100_ms)
        .withBlockSize (4410)
        .process();
    verify (usedSignal->popDSP());
}

HART_TEST ("DSP Contracts - Rendered by AudioBuffer")
{
    const hart::CLIConfig& cliConfig = hart::CLIConfig::getInstance();

    hart::AudioBuffer<float> buffer (
        cliConfig.getDefaultNumOutputChannels(),
        hart::roundToSizeT (cliConfig.getDefaultSampleRateHz() * cliConfig.getDefaultRenderDurationSeconds()),
        cliConfig.getDefaultSampleRateHz()
    );
    buffer.clear();

    // All default
    DSPContractChecker dspContractChecker = DSPContractChecker();
    buffer.processWith (dspContractChecker, hart::CLIConfig::getInstance().getGefaultBlockSizeFrames());
    dspContractChecker.verify();

    // Render in a single large block
    dspContractChecker = DSPContractChecker().withExpectedMaxBlockSize (buffer.getNumFrames());
    buffer.processWith (dspContractChecker);
    dspContractChecker.verify();

    // Block size of 1 frame
    constexpr double shortBufferSizeSeconds = 0.001_s;  // Rendering long pieces of audio takes way too much time with 1-frame blocks
    dspContractChecker = DSPContractChecker()
        .withExpectedMaxBlockSize (1)
        .withExpectedRenderedDuration (shortBufferSizeSeconds);
    const size_t shortBufferSizeFrames = hart::roundToSizeT (buffer.getSampleRateHz() * shortBufferSizeSeconds);
    buffer.setNumFrames (shortBufferSizeFrames);
    buffer.processWith (dspContractChecker, 1);
    dspContractChecker.verify();

    // All custom
    buffer = hart::AudioBuffer<float> (3, hart::roundToSizeT (45.658_kHz * 347_ms), 45.658_kHz);
    buffer.clear();
    dspContractChecker = DSPContractChecker()
        .withExpectedMaxBlockSize (123)
        .withExpectedChannelLayout (3, 3)
        .withExpectedSampleRate (45.658_kHz)
        .withExpectedRenderedDuration (347_ms);
    buffer.processWith (dspContractChecker, 123);
    dspContractChecker.verify();
}

HART_TEST ("Signal Contracts - Test Runner")
{
    auto signalContractChecker = SignalContractChecker();
    std::unique_ptr<hart::SignalBase<float>> usedSignal;
    processAudioWith (GainDb (0_dB))
        .withInputSignal (std::move (signalContractChecker))
        .saveInputSignalTo (usedSignal)
        .process();
    verify (usedSignal);

    signalContractChecker = SignalContractChecker()
        .withExpectedMaxBlockSize (123)
        .withExpectedNumChannels (9)
        .withExpectedSampleRate (45.658_kHz)
        .withExpectedRenderedDuration (347_ms);
    processAudioWith (GainDb (0_dB))
        .withLabel ("All custom")
        .withInputSignal (std::move (signalContractChecker))
        .saveInputSignalTo (usedSignal)
        .withSampleRate (45.658_kHz)
        .withInputChannels (9)
        .withOutputChannels (9)
        .withBlockSize (123)
        .withDuration (347_ms)
        .process();
    verify (usedSignal);

    signalContractChecker = SignalContractChecker()
        .withExpectedMaxBlockSize (1)
        .withExpectedRenderedDuration (1_ms);
    processAudioWith (GainDb (0_dB))
        .withLabel ("Block size of 1 frame")
        .withInputSignal (std::move (signalContractChecker))
        .saveInputSignalTo (usedSignal)
        .withBlockSize (1)
        .withDuration (1_ms)
        .process();
    verify (usedSignal);

    signalContractChecker = SignalContractChecker()
        .withExpectedSampleRate (44100_Hz)
        .withExpectedRenderedDuration (100_ms)
        .withExpectedMaxBlockSize (4410);
    processAudioWith (GainDb (0_dB))
        .withLabel ("Render in a single large block")
        .withInputSignal (std::move (signalContractChecker))
        .saveInputSignalTo (usedSignal)
        .withSampleRate (44100_Hz)
        .withDuration (100_ms)
        .withBlockSize (4410)
        .process();
    verify (usedSignal);
}

HART_TEST ("Signal Contracts - Hosted by a Matcher")
{
    // Note: as we cannot extract the reference signal from a matcher
    // to do post-render verification, the render-related stuff
    // like number of process() calls and observed rendered time
    // is a blind spot for now.

    auto signalContractChecker = SignalContractChecker();
    std::unique_ptr<hart::SignalBase<float>> usedSignal;
    processAudioWith (GainDb (0_dB))
        .withInputSignal (Silence())
        .expectTrue (EqualsTo (std::move (signalContractChecker)))
        .process();

    signalContractChecker = SignalContractChecker()
        .withExpectedMaxBlockSize (123)
        .withExpectedNumChannels (9)
        .withExpectedSampleRate (45.658_kHz)
        .withExpectedRenderedDuration (347_ms);
    processAudioWith (GainDb (0_dB))
        .withLabel ("All custom")
        .withInputSignal (Silence())
        .expectTrue (EqualsTo (std::move (signalContractChecker)))
        .withSampleRate (45.658_kHz)
        .withInputChannels (9)
        .withOutputChannels (9)
        .withBlockSize (123)
        .withDuration (347_ms)
        .process();

    signalContractChecker = SignalContractChecker()
        .withExpectedMaxBlockSize (1)
        .withExpectedRenderedDuration (1_ms);
    processAudioWith (GainDb (0_dB))
        .withLabel ("Block size of 1 frame")
        .withInputSignal (Silence())
        .expectTrue (EqualsTo (std::move (signalContractChecker)))
        .withBlockSize (1)
        .withDuration (1_ms)
        .process();

    signalContractChecker = SignalContractChecker()
        .withExpectedSampleRate (44100_Hz)
        .withExpectedRenderedDuration (100_ms)
        .withExpectedMaxBlockSize (4410);
    processAudioWith (GainDb (0_dB))
        .withLabel ("Render in a single large block")
        .withInputSignal (Silence())
        .expectTrue (EqualsTo (std::move (signalContractChecker)))
        .withSampleRate (44100_Hz)
        .withDuration (100_ms)
        .withBlockSize (4410)
        .process();
}

HART_TEST ("Signal Contracts - Rendered by AudioBuffer")
{
    const hart::CLIConfig& cliConfig = hart::CLIConfig::getInstance();

    hart::AudioBuffer<float> buffer (
        cliConfig.getDefaultNumInputChannels(),
        hart::roundToSizeT (cliConfig.getDefaultSampleRateHz() * cliConfig.getDefaultRenderDurationSeconds()),
        cliConfig.getDefaultSampleRateHz()
    );

    // All default
    auto signalContractChecker = SignalContractChecker();
    buffer.fillWith (signalContractChecker, cliConfig.getGefaultBlockSizeFrames());
    signalContractChecker.verify();

    // Render in a single large block
    signalContractChecker = SignalContractChecker().withExpectedMaxBlockSize (buffer.getNumFrames());
    buffer.fillWith (signalContractChecker);
    signalContractChecker.verify();

    // Block size of 1 frame
    signalContractChecker = SignalContractChecker()
        .withExpectedMaxBlockSize (1)
        .withExpectedRenderedDuration (1_ms);
    constexpr double shortBufferSizeSeconds = 0.001_s;  // Longer durations will result in test taking a bit too long to complete
    const size_t shortBufferSizeFrames = hart::roundToSizeT (buffer.getSampleRateHz() * shortBufferSizeSeconds);
    buffer.setNumFrames (shortBufferSizeFrames);
    buffer.fillWith (signalContractChecker, 1);
    signalContractChecker.verify();

    // All custom
    buffer = hart::AudioBuffer<float> (3, hart::roundToSizeT (45.658_kHz * 347_ms), 45.658_kHz);
    signalContractChecker = SignalContractChecker()
        .withExpectedMaxBlockSize (123)
        .withExpectedNumChannels (3)
        .withExpectedSampleRate (45.658_kHz)
        .withExpectedRenderedDuration (347_ms);
    buffer.fillWith (signalContractChecker, 123);
    signalContractChecker.verify();
}

HART_TEST ("Matcher Contracts")
{
    auto matcherContractCheckerA = MatcherContractChecker()
        .withMatchResult (true)
        .withPerBlockSupport (true);
    auto matcherContractCheckerB = MatcherContractChecker (matcherContractCheckerA)
        .withPerBlockSupport (false);
    auto matcherContractCheckerC = MatcherContractChecker (matcherContractCheckerA)
        .withMatchResult (false);
    auto matcherContractCheckerD = MatcherContractChecker (matcherContractCheckerB)
        .withMatchResult (false);

    processAudioWith (GainDb (0_dB))
        .withLabel ("All default")
        .expectTrue (std::move (matcherContractCheckerA))
        .expectTrue (std::move (matcherContractCheckerB))
        .expectFalse (std::move (matcherContractCheckerC))
        .expectFalse (std::move (matcherContractCheckerD))
        .process();

    matcherContractCheckerA = MatcherContractChecker()
        .withExpectedMaxBlockSize (123)
        .withExpectedChannelLayout (7, 7)
        .withExpectedSampleRate (45.658_kHz)
        .withExpectedCheckedDuration (347_ms)
        .withMatchResult (true)
        .withPerBlockSupport (true);
    matcherContractCheckerB = MatcherContractChecker (matcherContractCheckerA)
        .withPerBlockSupport (false);
    matcherContractCheckerC = MatcherContractChecker (matcherContractCheckerA)
        .withMatchResult (false);
    matcherContractCheckerD = MatcherContractChecker (matcherContractCheckerB)
        .withMatchResult (false);

    processAudioWith (GainDb (0_dB))
        .withLabel ("All custom")
        .withSampleRate (45.658_kHz)
        .withInputChannels (7)
        .withOutputChannels (7)
        .withBlockSize (123)
        .withDuration (347_ms)
        .expectTrue (std::move (matcherContractCheckerA))
        .expectTrue (std::move (matcherContractCheckerB))
        .expectFalse (std::move (matcherContractCheckerC))
        .expectFalse (std::move (matcherContractCheckerD))
        .process();

    matcherContractCheckerA = MatcherContractChecker()
        .withExpectedMaxBlockSize (1)
        .withExpectedCheckedDuration (1_ms)
        .withMatchResult (true)
        .withPerBlockSupport (true);
    matcherContractCheckerB = MatcherContractChecker (matcherContractCheckerA)
        .withPerBlockSupport (false);
    matcherContractCheckerC = MatcherContractChecker (matcherContractCheckerA)
        .withMatchResult (false);
    matcherContractCheckerD = MatcherContractChecker (matcherContractCheckerB)
        .withMatchResult (false);

    processAudioWith (GainDb (0_dB))
        .withLabel ("Block size of 1 frame")
        .withBlockSize (1)
        .withDuration (1_ms)
        .expectTrue (std::move (matcherContractCheckerA))
        .expectTrue (std::move (matcherContractCheckerB))
        .expectFalse (std::move (matcherContractCheckerC))
        .expectFalse (std::move (matcherContractCheckerD))
        .process();

    matcherContractCheckerA = MatcherContractChecker()
        .withExpectedSampleRate (44100_Hz)
        .withExpectedCheckedDuration (100_ms)
        .withExpectedMaxBlockSize (4410)
        .withMatchResult (true)
        .withPerBlockSupport (true);
    matcherContractCheckerB = MatcherContractChecker (matcherContractCheckerA)
        .withPerBlockSupport (false);
    matcherContractCheckerC = MatcherContractChecker (matcherContractCheckerA)
        .withMatchResult (false);
    matcherContractCheckerD = MatcherContractChecker (matcherContractCheckerB)
        .withMatchResult (false);

    processAudioWith (GainDb (0_dB))
        .withLabel ("Render in a single large block")
        .withSampleRate (44100_Hz)
        .withDuration (100_ms)
        .withBlockSize (4410)
        .expectTrue (std::move (matcherContractCheckerA))
        .expectTrue (std::move (matcherContractCheckerB))
        .expectFalse (std::move (matcherContractCheckerC))
        .expectFalse (std::move (matcherContractCheckerD))
        .process();
}

HART_TEST ("Envelope contracts")
{
    processAudioWith (GainDb (0_dB).withEnvelope (GainDb::gainDb, EnvelopeContractChecker()))
        .withLabel ("All Defaults")
        .process();

    auto envelopeContractChecker = EnvelopeContractChecker()
        .withExpectedSampleRate (45.658_kHz)
        .withExpectedRenderedDuration (347_ms)
        .withExpectedMaxBlockSize (123);
    
    processAudioWith (GainDb (0_dB).withEnvelope (GainDb::gainDb, std::move (envelopeContractChecker)))
        .withLabel ("All Custom")
        .withSampleRate (45.658_kHz)
        .withInputChannels (7)
        .withOutputChannels (7)
        .withBlockSize (123)
        .withDuration (347_ms)
        .process();

    envelopeContractChecker = EnvelopeContractChecker()
        .withExpectedMaxBlockSize (1)
        .withExpectedRenderedDuration (1_ms);
    processAudioWith (GainDb (0_dB).withEnvelope (GainDb::gainDb, std::move (envelopeContractChecker)))
        .withLabel ("Block size of 1 frame")
        .withBlockSize (1)
        .withDuration (1_ms)
        .process();

    envelopeContractChecker = EnvelopeContractChecker()
        .withExpectedSampleRate (44100_Hz)
        .withExpectedRenderedDuration (100_ms)
        .withExpectedMaxBlockSize (4410);
    processAudioWith (GainDb (0_dB).withEnvelope (GainDb::gainDb, std::move (envelopeContractChecker)))
        .withLabel ("Render in a single large block")
        .withSampleRate (44100_Hz)
        .withDuration (100_ms)
        .withBlockSize (4410)
        .process();
}

HART_TEST ("Metric contracts - Single channel")
{
    using Slice = hart::Slice;
    constexpr hart::Unit dB = hart::Unit::dB;
    constexpr hart::Unit Hz = hart::Unit::Hz;

    auto metric = SingleChannelMetricContractChecker()
        .withLabel ("All default");

    metric().get();
    metric.verify();

    metric.clear()
        .withReportedNumChannels (10)
        .withLabel ("Multi-channel");

    metric().get();
    metric.verify();

    metric.clear()
        .withDefaultChannels ({1, 3, 5, 7})
        .withReportedNumChannels (10)
        .withLabel ("Obscure default channel subset");

    metric().get();
    metric.verify();

    metric.clear()
        .withDefaultChannels ({1, 3, 5, 7})
        .withExpectedChannels ({3})
        .withReportedNumChannels (10)
        .withLabel ("Expecting specific single channel, ignoring defaults");

    metric().ch (3).get();
    metric.verify();

    metric.clear()
        .withDefaultChannels ({1, 3, 5, 7})
        .withExpectedChannels ({2, 4, 0, 6, 8})
        .withReportedNumChannels (10)
        .withLabel ("Expecting specific channels, ignoring defaults");

    metric().ch ({2, 4, 0, 6, 8}).get();
    metric.verify();

    metric.clear()
        .withExpectedUnit (Hz)
        .withLabel ("Expecting specific (Hz) unit");

    metric().as (Hz).get();
    metric.verify();

    metric.clear()
        .withExpectedUnit (dB)
        .withLabel ("Expecting specific (dB) unit");

    metric().as (dB).get();
    metric.verify();

    metric.clear()
        .withExpectedSlice (Slice::frames (123, 456))
        .withLabel ("Expecting specific slice in frames");

    metric().at (Slice::frames (123, 456)).get();
    metric.verify();

    metric.clear()
        .withExpectedSlice (Slice::time (5_ms, 15_ms))
        .withLabel ("Expecting specific slice in seconds");

    metric().at (Slice::time (5_ms, 15_ms)).get();
    metric.verify();
}

HART_TEST ("Metric contracts - Channel pair")
{
    using Slice = hart::Slice;
    constexpr hart::Unit seconds = hart::Unit::seconds;
    constexpr hart::Unit linear = hart::Unit::linear;

    auto metric = ChannelPairMetricContractChecker()
        .withLabel ("All default");

    metric().get();
    metric.verify();

    metric.clear()
        .withReportedNumChannels (10, 10)
        .withLabel ("Multi-channel");

    metric().get();
    metric.verify();

    metric.clear()
        .withDefaultChannelPairs ({{1, 1}, {3, 2}, {5, 5}, {7, 4}})
        .withReportedNumChannels (10, 10)
        .withLabel ("Obscure default channel subset");

    metric().get();
    metric.verify();

    metric.clear()
        .withExpectedChannelPairs ({{3, 3}})
        .withReportedNumChannels (10, 10)
        .withLabel ("Expecting specific single channel pair, as a single value");

    metric().ch (3).get();
    metric.verify();

    metric.clear()
        .withExpectedChannelPairs ({{3, 3}})
        .withReportedNumChannels (10, 10)
        .withLabel ("Expecting specific single channel pair, as a list of one single value");

    metric().ch ({3}).get();
    metric.verify();

    metric.clear()
        .withExpectedChannelPairs ({{3, 3}})
        .withReportedNumChannels (10, 10)
        .withLabel ("Expecting specific single channel pair, as an explicit list of one matched pair");

    metric().ch ({{3, 3}}).get();
    metric.verify();

    metric.clear()
        .withExpectedChannelPairs ({{3, 1}})
        .withReportedNumChannels (10, 10)
        .withLabel ("Expecting specific single channel pair, as an explicit list of one unmatched pair");

    metric().ch ({{3, 1}}).get();
    metric.verify();

    metric.clear()
        .withExpectedChannelPairs ({{3, 3}, {1, 1}})
        .withReportedNumChannels (10, 10)
        .withLabel ("Expecting two specific single channel pairs, as an implicit list of two matched pairs");

    // This is indeed 2 pairs, not to be confused with
    // .ch({{3, 1}}), which is one unmatched pair.
    metric().ch ({3, 1}).get();
    metric.verify();

    metric.clear()
        .withExpectedChannelPairs ({{2, 4}, {0, 6}, {8, 8}})
        .withReportedNumChannels (10, 10)
        .withLabel ("Expecting multiple specific channel pairs");

    metric().ch ({{2, 4}, {0, 6}, {8, 8}}).get();
    metric.verify();

    metric.clear()
        .withExpectedUnit (seconds)
        .withLabel ("Expecting specific (seconds) unit");

    metric().as (seconds).get();
    metric.verify();

    metric.clear()
        .withExpectedUnit (linear)
        .withLabel ("Expecting specific (linear) unit");

    metric().as (linear).get();
    metric.verify();

    metric.clear()
        .withExpectedSlice (Slice::freq (111_Hz, 2.222_kHz))
        .withLabel ("Expecting specific slice in Hz");

    metric().at (Slice::freq (111_Hz, 2.222_kHz)).get();
    metric.verify();

    metric.clear()
        .withExpectedSlice (Slice::bins (100, 500))
        .withLabel ("Expecting specific slice in bins");

    metric().at (Slice::bins (100, 500)).get();
    metric.verify();
}
