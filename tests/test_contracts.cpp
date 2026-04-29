#include "hart.hpp"
#include "dsp_contract_checker.hpp"

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
