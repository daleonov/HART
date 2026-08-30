# HART

HART is a header-only C++11 framework for testing audio DSP output. It helps audio developers automate audio signal-driven tests, like regression, latency, peak, stereo etc, without depending on frameworks like JUCE, gtest, or Catch2.

📚 Docs: https://daleonov.github.io/HART/

🎓 Tutorial: https://daleonov.github.io/HART/TestingYourDspInHart.html

🍲 DSP testing cookbook: https://daleonov.github.io/HART/DSPTestingCookbook.html

📺 Episode on HART at The Audio Programmer channel: https://www.youtube.com/live/f4yMv35pVDQ?si=8nB1wVB9uHt4QPO6

## What is HART?

HART is C++ audio DSP testing framework. Unlike common unit testing frameworks, it's made specifically for testing audio output of the DSP code. It helps with tests like:

 - Checking various audio properties of audio output, like dynamic range, stereo width, true peaks etc
 - Regression (cancellation) testing against pre-recorded "golden" outputs
 - Measuring actual latency vs reported latency
 - Ensuring consistency across different sample rates and block sizes

...and much more. For examples of test cases, see [DSP Testing Cookbook](https://daleonov.github.io/HART/DSPTestingCookbook.html).

## Can you use HART in your project?

Here's a few facts about HART that will hopefully answer that question:

 - C++11 compatible
 - Header only
 - CMake support
 - Standalone framework - no JUCE, gtest, Catch2 etc required
 - Free, open-source, MIT license
 - Minimal dependencies, already included in repo
 - All classes are thoroughly documented, with a few extra articles on top
 - Compatible with all major desktop platforms and compilers
 - Not vibe-coded

## What will testing with HART give you?

There are a number of tests that a lot of digital audio vendors do manually, like:

 - Catching regression during DSP code refactors and optimisations
 - Making sure the audio is consistent across different sample rates and block sizes
 - Detecting various issues like accidentally collapsing stereo to mono or post-instantiation glitches
 - Ensuring all the controls do what they're supposed to do
 - CPU performance benchmarking

HART can help you automate these sort of tests, freeing your critical listening time to the tests that matter the most, as in "does this sound good?" or "does it sound like that thing we're modelling?".

## Built-in testing tools

HART comes with built-in signals, matchers and metrics for common audio DSP test cases. You can use them directly, combine them into DSP chains, or use the metrics inside custom function-based matchers. You can also make your own ones, either by subclassing, or via functional programming.

### Matchers

Matchers are similar to their namesakes from Catch2. In HART, they represent some complete test assertion statements, i.e. combining what do we measure, what do we compare it to, and how do we compare it. 

| Matcher               | What it checks
| --------------------- | --------------
| `EqualsTo`            | Sample-by-sample equality against another signal, within a tolerance
| `PeaksAt`             | Sample peaks match an expected level
| `PeaksBelow`          | Sample peaks stay below some threshold
| `TruePeaksBelow`      | Estimated inter-sample peaks stay below some threshold
| `CorrelationAbove`    | Input and output are correlated above a requested value
| `FundamentalEquals`   | Estimated fundamental frequency matches the expected value
| `LatencyBelow`        | Measured latency stays below a specified time amount
| `NoDenormals`         | Output contains no subnormal floating-point samples
| `PolarityPreserved`   | Output retains the input polarity, instead of flipping it
| `MatcherFunction`     | Inline custom matcher from a lambda or callable object

### Metrics

Metrics are made for constructing more granular statements, as opposed to matchers. Generally, they represent some commonly used audio features, or derived versions of those. Usually, metrics are calculated per channel, or pair of channels, and can then be reduced to a scalar via built-in reducers like `min()`, `max()`, `nth()`, `percentile()` and others. Also, for most of those, a specific unit can be requested, and they can be applied to a specified slice of audio buffer, spectrum or IR.

| Metric                       | Units                | Domain    | What it measures
| ---------------------------- | -------------------- | --------- | ----------------
| `samplePeak`                 | linear, dB           | time      | Highest absolute sample peak value
| `truePeak`                   | linear, dB           | time      | Estimated inter-sample peak level, based on ITU-R BS.1770-5
| `rms`                        | linear, dB           | time, FFT | Root mean square level
| `crestFactor`                | linear, dB           | time      | Peak-to-RMS ratio, a.k.a crest factor, useful for dynamic range checks
| `zcr`                        | Hz                   | time      | Zero-crossing rate
| `channelCorrelation`         | none                 | time      | Correlation between channels
| `maxCrossCorrelation`        | none                 | time      | Best correlation between two signals within a lag window
| `lagAtMaxCrossCorrelation`   | frames, seconds      | time      | Lag at which the best cross-correlation occurs, useful for estimating latency
| `esr`                        | ratio                | time      | Error-to-signal ratio against a reference audio
| `snr`                        | ratio, dB            | time      | Signal-to-noise ratio against a reference audio
| `snra`                       | ratio, dB            | time      | Signal-to-aliasing-noise ratio against a high-sample-rate reference
| `thd`                        | ratio, dB, percent   | FFT       | Total harmonic distortion in a spectrum, assuming pure sine stimulus
| `spectralCentroid`           | Hz                   | FFT       | Spectral centroid, useful for expressing perceived brightness of audio
| `spectralFlatness`           | linear, dB           | FFT       | Spectral flatness, iseful for expressing how noise-like or tonal a spectrum is
| `spectralLogLogSlope`        | none, dB/oct, dB/dec | FFT       | Spectral tilt in log-frequency/log-magnitude space, assuming coloured noise-like spectrum
| `logSpectralDistance`        | dB                   | FFT       | Distance (difference) between two smoothed spectra
| `loudestBinFrequency`        | Hz                   | FFT       | Frequency of the loudest FFT bin
| `loudestBinMagnitude`        | linear, dB           | FFT       | Magnitude of the loudest FFT bin
| `interpolatedPeakFrequency`  | Hz                   | FFT       | Interpolated frequency of the loudest FFT bin
| `quinns2`                    | Hz                   | FFT       | Fundamental frequency estimation, using Quinn's second estimator
| `rt60`                       | frames, seconds      | IR        | Reverb decay time estimated from an impulse response, based on ISO 3382
| `centreTime`                 | frames, seconds      | IR        | Energy centre of an impulse response

### Signals

Signals are stimuli for HART test cases. Signals can be processed with built-in DSP primitives, like `Gain`, `TimeShift`, `Flip` etc, either at all channels, or selected channels only. They also can have Gain (and other DSP) envelopes. Also, multiple Signals can be mixed together, which gives you a lot of flexibility for procedurally generated audio input. 

| Signal                      | What it generates
| --------------------------- | -----------------
| `Silence`                   | A sequence of zeros
| `SineWave`                  | Fixed-frequency sine wave
| `SineSweep`                 | Sine sweep (chirp), log or linear, with flexible settings
| `Sawtooth`                  | Band-limited sawtooth wave
| `WhiteNoise`                | White noise, constructed in time domain, with parametric seed
| `PinkNoise`                 | Pink noise, constructed in time domain, with parametric seed
| `Spectrum::colouredNoise()` | Ideal coloured noise, constructed in frequency domain, with parametric seed
| `Impulse`                   | Single-sample impulse (`{1, 0, 0, 0, ...}` sequence)
| `DC`                        | Constant-value signal, a.k.a. "direct current"
| `NyquistSignal`             | Alternating `{+1, -1}` signal at Nyquist frequency
| `WavFile`                   | Audio loaded from a wav file, with re-sampling support
| `AudioBufferSignal`         | Signal produced from an existing `AudioBuffer` in RAM
| `SignalFunction`            | Custom signal, generated by a lambda or a callable object
| `MixedSignal`               | Combined signal, produced by adding or subtracting other signals

## How do you integrate HART in your workflow?

 - It doesn't require you to modify your existing DSP code, it interfaces with it by wrapping it, see [TestingYourDspInHart](https://daleonov.github.io/HART/TestingYourDspInHart.html)
 - HART test suites are intended to be a standalone binary, that you run as a part of your pipeline, see [Setting Up Your Project To Use HART](https://daleonov.github.io/HART/SettingUpYourProjectToUseHART.html)
 - You can start by adding simple tests that apply to pretty much every type of DSP effect, see [DSP Testing Cookbook](https://daleonov.github.io/HART/DSPTestingCookbook.html)

## Need help with integrating HART?

HART is completely free to use, and you're welcome to start using it any moment. But if your team is (understandably) busy with something else, I can offer some hands-on help or consulting for integrating HART into your company's pipeline. My contacts are on [my github page](https://github.com/daleonov), so feel free to reach out. Best ways would probably be gmail or LinkedIn.

## Test examples

Here's a couple of examples of validating audio behaviour or a DSP effect. For more examples, see [DSP Testing Cookbook](https://daleonov.github.io/HART/DSPTestingCookbook.html).

```cpp
HART_TEST_WITH_TAGS ("OutputLimiter - Limits sample peaks", "[limiter][basics]")
{
    for (double inputLevelDb : {-3_dB, -0.5_dB, 0_dB, 0.5_dB, 3_dB, 12_dB, 30_dB})
    {
        processAudioWith (OutputLimiterHartWrapper())
            .withLabel (HART_STR ("Input level " << inputLevelDb << " dB"))
            .withInputSignal (WhiteNoise() >> GainDb (inputLevelDb))
            .inStereo()
            .expectTrue (PeaksBelow (-1_dB))
            .expectTrue (TruePeaksBelow (0_dB))
            .process();
    }
}

HART_TEST_WITH_TAGS ("OutputLimiter - Doesn't collapse stereo to mono", "[limiter][stereo]")
{
    using hart::channelCorrelation;
    using std::abs;

    processAudioWith (OutputLimiterHartWrapper())
        .withInputSignal (WhiteNoise())
        .inStereo()
        .expectTrue ([] (const AudioBuffer& output) { return HART_LT (abs (channelCorrelation (output)), 0.5); }, "Output L and R channels are different")
        .process();
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

[   <3   ] [   1 ms] GainDb - GainDb Values - passed
[   <3   ] [   1 ms] GainDb - Channel Layouts - passed
[   <3   ] [   1 ms] HardClip - Threshold Values - passed
[   <3   ] [   3 ms] DSP Chains - Basic Gain - passed
[   <3   ] [   1 ms] DSP Chains - Order Matters - passed
[   <3   ] [  85 ms] DSP Chains - Long Chains - passed
[   <3   ] [   2 ms] Envelope - Gain Envelope Regression - passed
[   <3   ] [ 466 us] Host - DSP Move, Copy and Transfer - passed

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

[  </3   ] [   1 ms] GainDb - GainDb Values - failed
-------------------------------------------
expectTrue() failed at "Attenuation"
Condition: PeaksAt (-2.5_dB, 0.001000)
Channel: 0
Frame: 11
Timestamp: 0.100 seconds
Sample value: 0.707941 (-3.0 dB)
Observed audio peaks at -3.0 dB
-------------------------------------------
[   <3   ] [   2 ms] GainDb - Channel Layouts - passed
[   <3   ] [   1 ms] HardClip - Threshold Values - passed
[   <3   ] [   1 ms] DSP Chains - Basic Gain - passed
[   <3   ] [   1 ms] DSP Chains - Order Matters - passed
[   <3   ] [  80 ms] DSP Chains - Long Chains
[  </3   ] [   1 ms] Envelope - Gain Envelope Regression - failed
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
[   <3   ] [ 506 us] Host - DSP Move, Copy and Transfer - passed

[ PASSED ] 6/8
[ FAILED ] 2/8
```

## Known issues, and important features yet to be implemented:
 - Doesn't support sidechain signals yet. The current workaround is to take advantage of multi-channel inputs.
 - Doesn't support MIDI inputs.
 - Multithreading, as in running test cases in a thread pool, is very possible, but not implemented yet.
 - Exception-free mode is technically there (see `HART_DO_NOT_THROW_EXCEPTIONS` flag), but not thoroughly tested.
