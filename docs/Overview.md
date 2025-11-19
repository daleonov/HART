# Overview

## What is HART?

HART is a testing framework for audio DSP. It's a header-only library, and compatible with C++11.

- Create a simple test signal: `auto mySignal = SineWave (440_Hz);`
- Create automation for any DSP: `auto myEnvelopeCurve = SegmentedEnvelope (-10_dB).hold (5_ms).rampTo (0_dB, 25_ms)`
- Create an effect for a signal with sample-accurate automation: `auto myGain = GainDb ().withEnvelope (GainDb::gainDb, myEnvelopeCurve);`
- Add DSP chain to a signal: `auto envelope = SineWave (440_Hz) >> HardClip (-6_dB) >> MyCustomDSP() >> GainDb().withEnvelope (GainDb::gainDb, myEnvelopeCurve) >> LPF (1.2_kHz);`

## Test case example

```cpp
HART_TEST_WITH_TAGS ("My Test Name", "[my][test][tags]")
{
	// Create effect automation
    const auto myEnvelopeCurve = SegmentedEnvelope (-10_dB)
        .hold (5_ms)
        .rampTo (0_dB, 25_ms, SegmentedEnvelope::Shape::sCurve)
        .hold (5_ms)
        .rampTo (-10_dB, 35_ms, SegmentedEnvelope::Shape::linear);

    // Play wav file through your effect and test the output
    processAudioWith (MyEffect().withEnvelope (GainLinear::someAutomatedParam, myEnvelopeCurve))
        .withLabel ("Pre-recorded wav test")
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
        .withLabel ("Generated signal test")
        .withValue (MyEffect::someParam, 67_percent)
        .inStereo()
        .withInputSignal (myTestSignal)
        .saveInputTo ("my_failed_test_input.wav")
        .saveOutputTo ("my_failed_test_output.wav")
        .expectTrue (PeaksAt (-3_dB))
        .expectFalse (equalsTo (SineWave (2.2_kHz) >> HardClip (-3_dB) >> GainDb (+1_dB)))
        .process();

    // Traditional unit test assertions are also supported
    HART_ASSERT_TRUE (MyEffect().isAwesome());
    HART_EXPECT_TRUE (someValue == someOtherValue);
}
```

## Output example

On pass:

```text
88                                         
88                                    ,d   
88                                    88   
88,dPPYba,   ,adPPYYba,  8b,dPPYba, MM88MMM
88P'    "8a  ""     `Y8  88P'   "Y8   88   
88       88  ,adPPPPP88  88           88   
88       88  88,    ,88  88           88,  
88       88  `"8bbdP"Y8  88           "Y888

[   <3   ] GainDb - GainDb Values - passed
[   <3   ] GainDb - Channel Layouts - passed
[   <3   ] HardClip - Threshold Values - passed
[   <3   ] DSP Chains - Basic Gain - passed
[   <3   ] DSP Chains - Order Matters - passed
[   <3   ] DSP Chains - Long Chains - passed
[   <3   ] Envelope - Gain Envelope Regression - passed
[   <3   ] Host - DSP Move, Copy and Transfer - passed

[ PASSED ] 8/8
```

On fail:

```text
88                                         
88                                    ,d   
88                                    88   
88,dPPYba,   ,adPPYYba,  8b,dPPYba, MM88MMM
88P'    "8a  ""     `Y8  88P'   "Y8   88   
88       88  ,adPPPPP88  88           88   
88       88  88,    ,88  88           88,  
88       88  `"8bbdP"Y8  88           "Y888

[  </3   ] GainDb - GainDb Values - failed
-------------------------------------------
expectTrue() failed at "Attenuation"
Condition: PeaksAt (-2.5_dB, 0.001000)
Channel: 0
Frame: 11
Timestamp: 0.100 seconds
Sample value: 0.707941 (-3.0 dB)
Observed audio peaks at -3.0 dB
-------------------------------------------
[   <3   ] GainDb - Channel Layouts - passed
[   <3   ] HardClip - Threshold Values - passed
[   <3   ] DSP Chains - Basic Gain - passed
[   <3   ] DSP Chains - Order Matters - passed
[   <3   ] DSP Chains - Long Chains - passed
[  </3   ] Envelope - Gain Envelope Regression - failed
-------------------------------------------
expectTrue() failed at "Envelope B"
Condition: EqualsTo (WavFile ("Gain Envelope B.wav", Loop::no), 0.000010)
Channel: 0
Frame: 1
Timestamp: 0.000 seconds
Sample value: 0.107372 (-19.4 dB)
Expected sample value: 0.104125 (-19.6 dB), difference: 0.003246 (-49.8 dB)
-------------------------------------------
expectTrue() failed at "Envelope C"
Condition: EqualsTo (WavFile ("Gain Envelope C.wav", Loop::no), 0.000010)
Channel: 0
Frame: 1
Timestamp: 0.000 seconds
Sample value: 0.311974 (-10.1 dB)
Expected sample value: 0.310784 (-10.2 dB), difference: 0.001190 (-58.5 dB)
-------------------------------------------
[   <3   ] Host - DSP Move, Copy and Transfer - passed

[ PASSED ] 6/8
[ FAILED ] 2/8
```

## What's next?

See @ref TestingYourDspInHart to learn how to use it.

Note that HART is still in its infancy! It still needs some time to land on somewhat final API, and become a complete and bug-free solution we all want it to be. Speaking of complete and bug-free, contributions are very welcome!

Github repo: https://github.com/daleonov/HART
