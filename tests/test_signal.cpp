#include "hart.hpp"

HART_DECLARE_ALIASES_FOR_FLOAT;

HART_TEST ("Signal - Fast Forward via skipTo()")
{
    // 1. Actual polarity flip, or GainLinear(-1.0) lands on a perfect phase.
    // 2. Setting starting phase in SineWave ctor lands on a somewhat accurate phase, but not perfect due to FP error and pi being pi.
    // 3. Changing phase via nudging in time domain - pretty meh phase, since we only have 1 frame resolution and FP error.
    // ...So the tolerance is pretty loose here.
    constexpr double toleranceLinear = 1.0e-2;

    processAudioWith (GainDb())
        .withLabel ("Skip zero seconds")
        .withInputSignal (SineWave().skipTo (0_s))
        .expectTrue (EqualsTo (SineWave(), toleranceLinear))
        .process();

    processAudioWith (GainDb())
        .withLabel ("Skip arbitrary small amount")
        .withInputSignal (SineWave().skipTo (123_ms))
        .expectFalse (EqualsTo (SineWave(), toleranceLinear))
        .process();

    processAudioWith (GainDb())
        .withLabel ("Skip arbitrary significant amount")
        .withInputSignal (SineWave().skipTo (1.23456_s))
        .expectFalse (EqualsTo (SineWave(), toleranceLinear))
        .process();

    processAudioWith (GainDb())
        .withSampleRate (44100_Hz)
        .withLabel ("Skip exactly one cycle at 44.1kHz")
        .withInputSignal (SineWave (60_Hz).skipTo (1.0 / 60_Hz))
        .expectTrue (EqualsTo (SineWave (60_Hz), toleranceLinear))
        .process();

    processAudioWith (GainDb())
        .withSampleRate (48000_Hz)
        .withLabel ("Skip exactly one cycle at 48kHz")
        .withInputSignal (SineWave (440_Hz).skipTo (1.0 / 440_Hz))
        .expectTrue (EqualsTo (SineWave (440_Hz), toleranceLinear))
        .process();

    processAudioWith (GainDb())
        .withLabel ("Skip exactly four cycles")
        .withInputSignal (SineWave (100_Hz).skipTo (4.0 / 100_Hz))
        .expectTrue (EqualsTo (SineWave (100_Hz), toleranceLinear))
        .process();

    processAudioWith (GainDb())
        .withLabel ("Skip a hundred full cycles")
        .withInputSignal (SineWave (123_Hz).skipTo (100.0 / 123_Hz))
        .expectTrue (EqualsTo (SineWave (123_Hz), toleranceLinear))
        .process();

    processAudioWith (GainDb())
        .withLabel ("Skip exactly half a cycle")
        .withInputSignal (SineWave (60_Hz).skipTo (0.5 / 60_Hz))
        .expectFalse (EqualsTo (SineWave (60_Hz), toleranceLinear))
        .expectTrue (EqualsTo (SineWave (60_Hz) >> GainLinear (-1.0), toleranceLinear))
        .expectTrue (EqualsTo (SineWave (60_Hz, hart::pi), toleranceLinear))
        .process();

    processAudioWith (GainDb())
        .withLabel ("Skip exactly half a cycle in reference signal")
        .withInputSignal (SineWave (440_Hz, hart::pi))
        .expectFalse (EqualsTo (SineWave (440_Hz), toleranceLinear))
        .expectTrue (EqualsTo (SineWave (440_Hz).skipTo (0.5 / 440_Hz), toleranceLinear))
        .expectTrue (EqualsTo (SineWave (440_Hz) >> GainLinear (-1.0), toleranceLinear))
        .process();

    processAudioWith (GainDb())
        .withLabel ("Skip to the end of signal")
        .withInputSignal (SineSweep (200_ms).withLoop (SineSweep::Loop::no).skipTo (201_ms))
        .expectTrue (PeaksAt (-oo_dB))
        .process();

    processAudioWith (GainDb())
        .withLabel ("Skiping multiple times")
        .withInputSignal (SineWave().skipTo (121_ms).skipTo (29.1_ms))
        .expectTrue (EqualsTo (SineWave().skipTo (90_ms).skipTo (35_ms).skipTo (25.1_ms), toleranceLinear))
        .expectFalse (EqualsTo (SineWave(), toleranceLinear))
        .process();
}

HART_TEST ("Signal - Unary Flip")
{
    processAudioWith (GainDb())
        .withLabel ("Unary minus")
        .withInputSignal (-SineWave (440_Hz))
        .expectFalse (EqualsTo (SineWave (440_Hz)))
        .expectTrue (EqualsTo (SineWave (440_Hz, hart::pi)))
        .process();

    processAudioWith (GainDb())
        .withLabel ("Unary tilda")
        .withInputSignal (~SineWave (5_kHz))
        .expectFalse (EqualsTo (SineWave (5_kHz)))
        .expectTrue (EqualsTo (SineWave (5_kHz, hart::pi)))
        .process();

    processAudioWith (GainDb())
        .withLabel ("Minus vs tilda")
        .withInputSignal (~SineWave (60_Hz))
        .expectFalse (EqualsTo (SineWave (60_Hz)))
        .expectTrue (EqualsTo (-SineWave (60_Hz)))
        .process();

    processAudioWith (GainDb())
        .withLabel ("Signal with DSP chain")
        .withInputSignal (-SineWave() >> HardClip (-3_dB))
        .expectFalse (EqualsTo (SineWave() >> HardClip (-3_dB)))
        .expectTrue (EqualsTo (SineWave() >> GainLinear (-1.0) >> HardClip (-3_dB)))
        .process();
}

HART_TEST ("Signal - Mixing Signals")
{
    processAudioWith (GainDb())
        .withLabel ("Adding two identical signals")
        .withInputSignal (SineWave() + SineWave())
        .expectFalse (EqualsTo (SineWave()))
        .expectTrue (EqualsTo (SineWave() >> GainLinear (2.0)))
        .process();

    processAudioWith (GainDb())
        .withLabel ("Subtracting two identical signals")
        .withInputSignal (SineWave() - SineWave())
        .expectTrue (PeaksAt (-oo_dB))
        .process();

    processAudioWith (GainDb())
        .withLabel ("Adding two out of phase signals")
        .withInputSignal (SineWave (440_Hz) + SineWave (440_Hz, hart::pi))
        .expectTrue (PeaksAt (-oo_dB))
        .process();

    processAudioWith (GainDb())
        .withLabel ("Subtracting two out of phase signals")
        .withInputSignal (SineWave (440_Hz) - SineWave (440_Hz, hart::pi))
        .expectTrue (EqualsTo (SineWave (440_Hz) >> GainLinear (2.0)))
        .process();

    processAudioWith (GainDb())
        .withLabel ("Order doesn't matter")
        .withInputSignal (SineWave (440_Hz) + SineWave (100_Hz))
        .expectFalse (EqualsTo (SineWave (440_Hz)))
        .expectFalse (EqualsTo (SineWave (100_Hz)))
        .expectTrue (EqualsTo (SineWave (100_Hz) + SineWave (440_Hz)))
        .process();

    processAudioWith (GainDb())
        .withLabel ("Adding a bunch of identical signals")
        .withInputSignal (SineWave() + SineWave() + SineWave() + SineWave() + SineWave())
        .expectFalse (EqualsTo (SineWave()))
        .expectTrue (EqualsTo (SineWave() >> GainLinear (5.0)))
        .process();

    processAudioWith (GainDb())
        .withLabel ("Preserving signal chains")
        .withInputSignal ((SineWave() >> GainLinear (1.23)) + (SineWave() >> GainLinear (4.56) >> GainLinear (3.21)))
        .expectFalse (EqualsTo (SineWave()))
        .expectTrue (EqualsTo (SineWave() >> GainLinear (1.23 + 4.56 * 3.21)))
        .process();
}

HART_TEST ("Signal - Sawtooth Frequency")
{
    processAudioWith (GainDb())
        .withLabel ("Normal use")
        .withInputSignal (Sawtooth())
        .expectTrue (PeaksAt (0_dB))
        .process();

    processAudioWith (GainDb())
        .withLabel ("Arbitraty phase")
        .withInputSignal (Sawtooth (1000_Hz, 1.2345_rad))
        .expectTrue (PeaksAt (0_dB))
        .expectFalse (EqualsTo (Sawtooth()))
        .process();

    processAudioWith (GainDb())
        .withLabel ("TwoPi starting phase")
        .withInputSignal (Sawtooth (1000_Hz, hart::twoPi))
        .expectTrue (PeaksAt (0_dB))
        .expectTrue (EqualsTo (Sawtooth()))
        .process();

    processAudioWith (GainDb())
        .withLabel ("Default Frequency")
        .withInputSignal (Sawtooth())
        .expectTrue (PeaksAt (0_dB))
        .expectTrue (FundamentalEquals (1000_Hz))
        .process();

    processAudioWith (GainDb())
        .withLabel ("Arbitrary Frequency")
        .withInputSignal (Sawtooth (440_Hz))
        .expectTrue (PeaksAt (0_dB))
        .expectTrue (FundamentalEquals (440_Hz))
        .process();
}

HART_TEST ("Signal - Sawtooth Peaks")
{
    const std::array<double, 7> sampleRatesHz = {44.1_kHz, 48_kHz, 88.2_kHz, 96_kHz, 192_kHz, 45.678_kHz, 57.689_kHz};
    const std::array<double, 8> frequenciesHz = {10_Hz, 20_Hz, 123_Hz, 440_Hz, 1_kHz, 3.456_kHz, 8_kHz, 9.876_kHz};
    constexpr double cyclesToGenerate = 10.0;
    constexpr double toleranceLinear = 5e-2;

    for (double sampleRateHz : sampleRatesHz)
        for (double frequencyHz : frequenciesHz)
            processAudioWith (GainDb())
                .withSampleRate (sampleRateHz)
                .withDuration (cyclesToGenerate / frequencyHz)
                .withLabel ("Peak at " + std::to_string (sampleRateHz) + " Hz SR, " + std::to_string (frequencyHz) + " Hz")
                .withInputSignal (Sawtooth (frequencyHz))
                .expectTrue (PeaksAt (0_dB, toleranceLinear))
                .process();
}
