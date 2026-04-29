#include "hart.hpp"
#include "dsp_contract_checker.hpp"

HART_DECLARE_ALIASES_FOR_FLOAT;

static void verify (const std::unique_ptr<hart::DSPBase<float>>& dsp)
{
    const DSPContractChecker* dspContractChecker = dynamic_cast<DSPContractChecker*> (dsp.get());
    HART_ASSERT_NE (dspContractChecker, nullptr);
    dspContractChecker->verify();
}

HART_TEST ("DSP Contracts - Test Runner")
{
    DSPContractChecker dspContractChecker = DSPContractChecker()
        .withExpectedMaxBlockSize (1024)
        .withExpectedChannelLayout (1, 1)
        .withExpectedSampleRate (44100_Hz)
        .withExpectedRenderedDuration (100_ms);
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

    dspContractChecker = DSPContractChecker().withExpectedMaxBlockSize (1);
    usedDsp = processAudioWith (std::move (dspContractChecker))
        .withLabel ("Block size of 1 frame")
        .withBlockSize (1)
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

    DSPContractChecker dspContractChecker = DSPContractChecker()
        .withExpectedMaxBlockSize (1024)
        .withExpectedChannelLayout (1, 1)
        .withExpectedSampleRate (44100_Hz)
        .withExpectedRenderedDuration (100_ms);
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

    dspContractChecker = DSPContractChecker().withExpectedMaxBlockSize (1);
    processAudioWith (GainDb (0_dB))
        .withLabel ("Block size of 1 frame")
        .withInputSignal (Silence() >> std::move (dspContractChecker))
        .saveInputSignalTo (usedSignal)
        .withBlockSize (1)
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
    hart::AudioBuffer<float> buffer (1, 4410, 44100_Hz);
    buffer.clear();

    // All default
    DSPContractChecker dspContractChecker = DSPContractChecker()
        .withExpectedMaxBlockSize (1024)
        .withExpectedChannelLayout (1, 1)
        .withExpectedSampleRate (44100_Hz)
        .withExpectedRenderedDuration (100_ms);
    buffer.processWith (dspContractChecker, 1024);
    dspContractChecker.verify();

    // Block size of 1 frame
    dspContractChecker = DSPContractChecker().withExpectedMaxBlockSize (1);
    buffer.processWith (dspContractChecker, 1);
    dspContractChecker.verify();

    // Render in a single large block
    dspContractChecker = DSPContractChecker()
        .withExpectedSampleRate (44100_Hz)
        .withExpectedRenderedDuration (100_ms)
        .withExpectedMaxBlockSize (4410);
    buffer.processWith (dspContractChecker);
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
