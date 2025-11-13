# Getting Started

HART is a testing framework for audio DSP.

- Create a simple test signal: `auto mySignal = SineWave (440_Hz);`
- Create automation for any DSP: `auto myEnvelopeCurve = SegmentedEnvelope (-10_dB).hold (5_ms).rampTo (0_dB, 25_ms)`
- Create an effect for a signal with sample-accurate automation: `auto myGain = GainDb ().withEnvelope (GainDb::gainDb, myEnvelopeCurve);`
- Add DSP chain to a signal: `auto envelope = SineWave (440_Hz) >> HardClip (-6_dB) >> MyCustomDSP() >> GainDb().withEnvelope (GainDb::gainDb, myEnvelopeCurve) >> LPF (1.2_kHz);`
- Test:

```cpp
HART_TEST_WITH_TAGS ("Ny Test Name", "[my][test][tags]")
{
	// Create effect automation
    const auto myEnvelopeCurve = SegmentedEnvelope (-10_dB)
        .hold (5_ms)
        .rampTo (0_dB, 25_ms, SegmentedEnvelope::Shape::sCurve)
        .hold (5_ms)
        .rampTo (-10_dB, 35_ms, SegmentedEnvelope::Shape::linear);

    // Play wav file through your effect and test the output
    processAudioWith (MyEffect().withEnvelope (GainLinear::someAutomatedParam, myEnvelopeCurve))
        .withInputSignal (WavFile ("my_test_input.wav"))
        .withSampleRate (48000_Hz)
        .withBlockSize (2048)
        .withValue (MyEffect::someOtherParam, -oo_dB)
        .withValue (MyEffect::andAnotherParam, 3.5_kHz)
        .withDuration (75_ms)
        .assertTrue (PeaksBelow (-1_dB))
        .expectTrue (EqualsTo (WavFile ("my_test_reference_output.wav")))
        .process();

    // Create test signal instead of using pre-rendered wav
    const auto myTestSignal = SineWave (2.2_kHz) >> GainDb().withEnvelope (GainDb::gainDb, SegmentedEnvelope) >> HardClip (-3_dB) >> HPF (100_Hz);

    // Play generated signal through your effect, save input and output audio if fails
    processAudioWith (MyEffect())
        .withValue (MyEffect::someParam, 67_percent)
        .inStereo()
        .withInputSignal (myTestSignal)
        .saveInputTo ("my_failed_test_input.wav")
        .saveOutputTo ("my_failed_test_output.wav")
        .expectTrue (PeaksAt (-3_dB))
        .expectFalse (equalsTo (SineWave(2.2_kHz) >> HardClip (-3_dB) >> GainDb (+1_dB)))
        .process();

    // Traditional unit test assertions are also supported
    HART_ASSERT_TRUE (MyEffect().isAwesome());
    HART_EXPECT_TRUE (someValue == someOtherValue);
}
```

See @ref hart::SineWave, @ref hart::GainDb
