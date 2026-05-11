@page DSPTestingCookbook DSP Testing Cookbook

# Introduction

This is a list of test ideas that you can use whEn coming up with test suites for your DSP projects. It's based on recurrent test cases and patterns I've found myself doing while doing automated tests for my clients' projects. I'll keep updating the article as HART evolves, and also as I discover useful ways to test various DSP algorithms.

**Examples note:** While HART is strictly C++11 compatible, I'll let myself use more modern C++ features in examples in this article, to keep things more neat and readable.

# Effect agnostic tests

There's a number of tests you can do, that fit pretty much any type of effect.

If you're familiar with [pluginval](https://github.com/Tracktion/pluginval) (and I hope you are!), it's a good example of a set of such tests. It doesn't really care what your plugin does. Your plugin crashes when passed a 0-sized block? It doesn't update a parameter when asked to do so? It's cooked then, and it doesn't matter whether it's a multi-band compressor or a Pultec EQ modeler.

Same idea with tests that involve rendering audio, and actually checking the audio (unlike aforementioned pluginval). Before you even think of diving into checking whether your compressor has a correct ratio, or whether your EQ changes the spectrum in a correct way, you might want to do a few rudimentary tests to make sure if your DSP code is even ready for more in-depth testing.

## Silence in - Silence out

This is pretty much a "Hello world" of DSP testing, in my opinion. If your DSP is not a synth or a sampler, if you feed zeros into it, it should probably output zeros, or at least something well below some reasonable threshold. Especially right after instantiation or reset or "prepare to play" type of callbacks, when some algos can briefly produce some junk audio before their states warm up properly.

On top of that, it's a pretty useful indicator that your audio test code compiles and runs properly, as it's usually one first test you'd write for your fresh "Audio Tests" target in your project.

```cpp
HART_TEST ("Silence in - Silence out")
{
    processAudioWith (MyDspWrapper())
        .withInputSignal (Silence())
        .expectTrue (EqualsTo (Silence()))
        .process();
}
```

Looks pretty dumb and simple? We can make it a little more robust. `EqualsTo()` compares signals sample-by sample withing a cetrain tolerance, and we have a tiny amount of noise, below that tolerance threshols, it can run under the radar. You could also use something like `PeaksBelow (-oo_dB)`, but it has the same flaw in this context. But you can check for zero crossing rate. If the noise is below the threshold, it will still cross the zero every now and then, exposing the sneaky noise. With this in mind:

```cpp
HART_TEST ("Silence in - Silence out")
{
    using hart::zcr;  // Zero Crossing Rate metric

    processAudioWith (MyDspWrapper())
        .withInputSignal (Silence())
        .expectTrue (EqualsTo (Silence()))
        .expectTrue (
            [] (const AudioBuffer& buffer)
                { return HART_FLOAT_EQ (zcr (buffer).get(), 0_Hz, 1e-8); },
            "ZCR is zero Hz"
            )
        .process();
}
```

It's also a good example of how you can take advantage of function-based matchers and built-in \ref Metrics to create highly-customized matchers like this ZCR check.

## Outputs audio

If a previous test passes, it's good news - unless your DSP always outputs silence, regardless of the input. To ensure it's not the case, it's a good idea to play something through it, and check if it's not outputting silence, when it's supposed to produce some meaningful audio.

You can try a few different signal types, like WhiteNoise ot SineSweep, and perhaps change a few parameters on your DSP, or keep it simple like in an example below.

```cpp
HART_TEST ("Outputs audio")
{
    processAudioWith (MyDspWrapper())
        .withInputSignal (SineWave())
        .expectFalse (EqualsTo (Silence()))
        .process();
}
```

## Modifies audio

Okay, so you have established that your DSP ouputs some audio, and not silence. Now we want to make sure it does something to the input signal. Here's an example of such a test from one of the projects I was working on recently.

```cpp
HART_TEST ("Saturation - Modifies audio")
{
    std::unique_ptr<hart::DSPBase<float>> saturationHartWrapperReusable = std::make_unique<SaturationHartWrapper>();

    saturationHartWrapperReusable = processAudioWith (std::move (saturationHartWrapperReusable))
        .withLabel ("SineWave")
        .withInputSignal (SineWave())
        .inStereo()
        .expectFalse (EqualsTo (SineWave()))
        .process();

    saturationHartWrapperReusable = processAudioWith (std::move (saturationHartWrapperReusable))
        .withLabel ("Sawtooth")
        .withInputSignal (Sawtooth())
        .inStereo()
        .expectFalse (EqualsTo (Sawtooth()))
        .process();

    saturationHartWrapperReusable = processAudioWith (std::move (saturationHartWrapperReusable))
        .withLabel ("WhiteNoise")
        .withInputSignal (WhiteNoise())
        .inStereo()
        .expectFalse (EqualsTo (WhiteNoise()))
        .process();
}
```

Obviously, in some cases you have to be extra careful with the choice of your input signal - if it's something like a limiter or a clipper, it may not do anything if the signal is not hot enough. And if you're feeding a basic sine wave into some filter, it may not be affected too, depending on its frequency.

If your DSP only adds some latency and/or merely changing its level and/or flips its polarity, i. e. preserves the waveform itself, but you expect this DSP to do more, this test will miss it. In this case, consider using [normalized cross-correlation](https://en.wikipedia.org/wiki/Cross-correlation) instead of a verbatim `EqualsTo`:

```cpp
HART_TEST ("Modifies audio - Correlation method")
{
    processAudioWith (std::move (MyDspWrapper()))
        .withLabel ("Sawtooth")
        .withInputSignal (Sawtooth())
        .inStereo()
        .expectFalse (CorrelationAbove (0.999))
        .process();
}
```

In some cases, error-to-signal ratio (ESR) can also be appropriate:

```cpp
HART_TEST ("Modifies audio - ESR method")
{
    using hart::esr;

    processAudioWith (std::move (MyDspWrapper()))
        .withLabel ("Sawtooth")
        .withInputSignal (Sawtooth())
        .inStereo()
        .expectTrue (
            [] (const auto& input, const auto& output) { return HART_FLOAT_GT (esr (input, output), 0.05); },
            "ESR > 5%"
            )
        .process();
}
```

Using ESR and correlation also lets gives you a quantitative way to express how different you expect the output to be from the input.

## Same settings - Same output

Even if your DSP is not fully deterministic, you probably want it to sound the same (or same-ish), when it renders same input audio while having same settings. Here's a somewhat naive implementation of this idea:

```cpp
HART_TEST ("Same settings - Same output")
{
    hart::AudioBuffer<float> referenceBuffer;

    auto myDspWrapperReuse = processAudioWith (MyDspWrapper())
        .withInputSignal (SineWave())
        .saveOutputTo (referenceBuffer)
        .process();

    constexpr size_t numPasses = 5;

    for (size_t i = 1; i <= numPasses; ++i)
    {
        myDspWrapperReuse = processAudioWith (std::move (myDspWrapperReuse))
            .withLabel (HART_STR ("Pass " << i << " out of " << numPasses))
            .withInputSignal (SineWave())
            .expectTrue (EqualsTo (AudioBufferSignal (referenceBuffer)))
            .process();
    }
}
```

A few things to point out here:
1. The DSP instance is re-used here, which is sometimes a good idea for performance reasons. However, if you can afford to instantiate a new DSP instance for each pass, you're welcome to do so.

2. If the DSP testee is stateful, it will carry on its accumulated state to each consecutive pass, which might affect its output slightly, but enough to cause a false alarm. If it's the case, you can avoid it either via instantiating a new DSP for each pass, as discussed in (1), or, better yet, request a reset via `withDspPreparation()` in every pass. And if your DSP cannot properly reset itself, then perhaps you should fix it.

3. `EqualsTo` can be a bit too strict in some cases, especially if the DSP is not fully deterministic, as it compares two pieces of audio frame-by-frame within a certain tolerance. For more relaxed checks, you can:

    - use `EqualsTo`, but with less tight tolerance;
    - use ESR (error-to-signal ratio) instead of comparing frame-by-frame, which is less strict in this scenario
    - use cross-correlation instead of comparing frame-by-frame, which is even less strict, as the implementation below ignores signal level and latency

4. We're using a very synthetic `SineWave` at unity gain as input signal. Obviously, you might want to choose a more meaningful input signal for your specific type of DSP. Or at least something non-stationary, like `SineSweep`.

5. We're testing effect at default settings, which is usually fine. But you might want to test your DSP at some non-default configuration, or even at a few different ones.

With all that in mind, a less naive version of this test may look something like this:

```cpp
HART_TEST ("Same settings - Same output")
{
    using Preparation = hart::Preparation;
    using hart::esr;

    hart::AudioBuffer<float> referenceBuffer;

    auto myDspWrapperReuse = processAudioWith (MyDspWrapper())
        .withDspPreparation (Preparation::prepare)
        .withValue (MyDspWrapper::someThresholdRelatedParam, -9_dB)
        .withInputSignal (SineSweep() >> GainDb (-3_dB)))
        .saveOutputTo (referenceBuffer)
        .process();

    constexpr size_t numPasses = 5;

    for (size_t i = 1; i <= numPasses; ++i)
    {
        constexpr double toleranceLinear = 0.01;

        myDspWrapperReuse = processAudioWith (std::move (myDspWrapperReuse))
            .withLabel (HART_STR ("Pass " << i << " out of " << numPasses))
            .withDspPreparation (Preparation::reset)
            .withValue (MyDspWrapper::someThresholdRelatedParam, -9_dB)
            .withInputSignal (SineSweep() >> GainDb (-3_dB)))
            .expectTrue (EqualsTo (AudioBufferSignal (referenceBuffer), toleranceLinear))
            .expectTrue (
                [] (const auto& input, const auto& output) { return HART_FLOAT_LT (esr (input, output), 0.05); },
                "ESR < 5%"
                )
            .process();
    }
}
```

## Different settings - Different output

What if one of the controls doesn't do anything? It's easy to miss if you have a lot of them. Here's a test ripped from one of my projects:

```cpp
HART_TEST ("CoreDSP Basics - Density control alters audio")
{
    using AudioBuffer = hart::AudioBuffer<float>;

    std::array<std::pair<double, double>, 3> densityChanges = {{
        {0.0, 100.0},
        {40.0, 60.0},
        {50.0, 51.0}
    }};

    for (auto [densityA, densityB] : densityChanges)
    {
        AudioBuffer bufferA;
        processAudioWith (CoreDspHartWrapper())
            .withValue (CoreDspHartWrapper::density, densityA)
            .withInputSignal (SineWave())
            .inStereo()
            .saveOutputTo (bufferA)
            .process();

        AudioBuffer bufferB;
        processAudioWith (CoreDspHartWrapper())
            .withValue (CoreDspHartWrapper::density, densityB)
            .withInputSignal (SineWave())
            .inStereo()
            .saveOutputTo (bufferB)
            .process();

        HART_EXPECT_NOT_EQUAL (bufferA, bufferB);
    }
}
```

I'm taking one of the controls (called "Density" here), and excercise 3 changes - from min to max value, then a moderate change, then the smallest change possible. With this particular DSP, I'm expecting even a tiniest change to the parameter to affect the audio. And in that test suite, I did a similar test for each of the plugin parameters, excercising them one by one, while keeping the rest at their default values.

The output audio is compared frame-by-frame withing some tolerance, i. e. the `HART_EXPECT_NOT_EQUAL (bufferA, bufferB)` statement is equivalent to `EqualsTo` matcher. As discussed previously, less strict metrics can also be used in this scenario.

This exact test can be expressed in a more "traditional" unit test way:

```cpp
HART_TEST ("SomeParam alters audio")
{
    using AudioBuffer = hart::AudioBuffer<float>;
    using Preparation = hart::Preparation;

    const hart::CLIConfig& cliConfig = hart::CLIConfig::getInstance();
    const double sampleRateHz = cliConfig.getSampleRateHz();
    const size_t blockSizeFrames = cliConfig.getDefaultBlockSizeFrames();
    const size_t renderDurationFrames = cliConfig.getDefaultSampleRateHz();
        hart::roundToSizeT (cliConfig.getDefaultSampleRateHz() * cliConfig.getDefaultRenderDurationSeconds());
    constexpr numChannels = 2;

    std::array<std::pair<double, double>, 3> someParamChanges = {{
        {0.0, 100.0},
        {40.0, 60.0},
        {50.0, 51.0}
    }};

    for (auto [someParamValueA, someParamValueB] : someParamChanges)
    {
        auto bufferA = AudioBuffer (numChannels, sampleRateHz, renderDurationFrames)
            .fillWith (SineWave(), blockSizeFrames);
        auto bufferB = AudioBuffer (bufferA);

        auto myDspWrapper = MyDspWrapper();
        myDspWrapper.setValue (MyDspWrapper::someParam, someParamValueA)
        bufferA.processWith (myDspWrapper, blockSizeFrames, Preparation::prepare);

        myDspWrapper.setValue (MyDspWrapper::someParam, someParamValueB)
        bufferB.processWith (myDspWrapper, blockSizeFrames, Preparation::reset);

        HART_EXPECT_NOT_EQUAL (bufferA, bufferB);
    }
}
```

Since we need to render 2 different pieces of audio with different configuration of the DSP instance, constructing a usual test runner via `processAudioWith()` just to capture the audio to buffer instances may seem a bit cumbersome. For such scenarios, HART provides you a way to take full control over your test case, and express your test case as a more traditional unit test.

There's a bit at the top querying hart::CLIConfig - this lets you change sample rate, block size and duration via CLI arguments, rather than hard-coding them. But you can just hard-code them, of course, if it suits you.

## No denormals

Pretty self-explanatory. Usually we want to avoid those [subnormal](https://en.wikipedia.org/wiki/Subnormal_number) floating point values, as they tend to give us a performance hit for no benefit. Here's a test throwing a few very quiet signals at some DSP algorithm, expecting no denormals in the output audio.

```cpp
HART_TEST ("No denormals")
{
    std::unique_ptr<hart::DSPBase<float>> coreDspHartWrapperReusable = std::make_unique<CoreDspHartWrapper>();

    coreDspHartWrapperReusable = processAudioWith (std::move (coreDspHartWrapperReusable))
        .withLabel ("Unity gain")
        .withInputSignal (SineWave())
        .inStereo()
        .expectTrue (NoDenormals())
        .process();

    coreDspHartWrapperReusable = processAudioWith (std::move (coreDspHartWrapperReusable))
        .withLabel ("-760 dB - Input expected to be all denormals")
        .withInputSignal (SineWave() >> GainLinear (1e-38))
        .inStereo()
        .expectTrue (NoDenormals())
        .process();

    coreDspHartWrapperReusable = processAudioWith (std::move (coreDspHartWrapperReusable))
        .withLabel ("-600 dB - Some denormals expected in the input")
        .withInputSignal (SineWave() >> GainLinear (1e-30))
        .inStereo()
        .expectTrue (NoDenormals())
        .process();

    coreDspHartWrapperReusable = processAudioWith (std::move (coreDspHartWrapperReusable))
        .withLabel ("-500 dB - Occasional denormals expected in the input")
        .withInputSignal (SineWave() >> GainLinear (1e-25))
        .inStereo()
        .expectTrue (NoDenormals())
        .process();

    coreDspHartWrapperReusable = processAudioWith (std::move (coreDspHartWrapperReusable))
        .withLabel ("Complete silence")
        .withInputSignal (SineWave() >> GainLinear (0.0))
        .inStereo()
        .expectTrue (NoDenormals())
        .process();
}
```

## Stereo in - Stereo out

Sometimes a mistake in DSP code may collapse stereo input to mono, i. e. left and right output channel contents accidentally become identical. Here's a simple test that checks it:

```cpp
HART_TEST ("Stereo in - Stereo out - M/S")
{
    processAudioWith (HART_DSP_SEQUENCE (MyDspWrapper() >> StereoToMidSide()))
        .withLabel ("Left and right out of phase")
        .withInputSignal (SineWave() >> GainLinear (-1.0).atChannel (Channel::left))
        .inStereo()
        .expectTrue (EqualsTo (Silence()).atChannel (MidSideChannel::mid))
        .expectFalse (EqualsTo (Silence()).atChannel (MidSideChannel::side))
        .process();

    processAudioWith (HART_DSP_SEQUENCE (MyDspWrapper() >> StereoToMidSide()))
        .withLabel ("True stereo signal")
        .withInputSignal (WhiteNoise())
        .inStereo()
        .expectFalse (EqualsTo (Silence()).atChannel (MidSideChannel::mid))
        .expectFalse (EqualsTo (Silence()).atChannel (MidSideChannel::side))
        .process();
}
```

Note how we're putting a Mid/Side converter (`StereoToMidSide`) after our DSP. You can chain your DSP with other DSP effects (built-in or your own) using `HART_DSP_SEQUENCE()`, or as a part of input signal's DSP chain. In this case, we want to express that side channel is not silent.

Alternatively, you can say that left and right channels are not perfectly correlated:

```cpp
HART_TEST ("Stereo in - Stereo out - L/R correlation")
{
    using std::abs;

    processAudioWith (MyDspWrapper())
        .withLabel ("Left and right out of phase")
        .withInputSignal (SineSweep() >> GainLinear (-1.0).atChannel (Channel::left))
        .inStereo()
        .expectTrue (
            [] (const AudioBuffer& output) { return HART_LT (channelCorrelation (output), 0.0); },
            "Left and Right are negatively correlated"
        )
        .process();

    processAudioWith (MyDspWrapper())
        .withLabel ("True stereo signal")
        .withInputSignal (WhiteNoise())
        .inStereo()
        .expectTrue (
            [] (const AudioBuffer& output) { return HART_LT (abs (channelCorrelation (output)), 0.99); },
            "Left and Right aren't perfectly correlated"
            )
        .process();
}
```

It involves completely different math, and also valid. Here, it uses `SineSweep` as input signal, unlike `SineWave` in previous example, to not confuse cross-correlation algorithm with a repetitive signal.

## Mono in - Mono out

Quite often we work with mono sources, but in stereo channel layout, i. e. left and right input channels are identical. And in some scenarios, we don't intend to add any stereo information to the signal. It's basically a reversed version of the previous test:

```cpp
HART_TEST ("Mono in - Mono out")
{
    processAudioWith (HART_DSP_SEQUENCE (MyDspWrapper() >> StereoToMidSide()))
        .withLabel ("Mid/Side method")
        .withInputSignal (SineWave())
        .inStereo()
        .expectTrue (EqualsTo (Silence()).atChannel (MidSideChannel::side))
        .expectFalse (EqualsTo (Silence()).atChannel (MidSideChannel::mid))
        .process();

    processAudioWith (MyDspWrapper())
        .withLabel ("Cross-correlation")
        .withInputSignal (SineSweep())
        .inStereo()
        .expectTrue (
            [] (const AudioBuffer& output) { return HART_GT (channelCorrelation (output), 0.999); },
            "Left and Right aren't perfectly correlated"
            )
        .process();
}
```

## Reported latency is correct

If the plugin reports it latency incorrectly to the DAW, it's pretty much impossible to notice it when you just listening your plugin in solo. It only reveals itself in a session with a bunch of audio tracks and various plugins, once PDC kicks in. And even then, you might not catch it. Or it can happen to some of your plugin's internal DSP chain components, and is especially pesky when you have latency-aware dry/wet mixers.

HART provides two different methods of measuring latency:

1. **Onset method.** It expects you to use an impulse, or something similar, as an input signal, and waits for the first non-zero (above a certain threshold) frame. Your DSP is expected to be in a flushed state, i. e. it's expected to output zeros until the moment it actially reacts to the input signal.

2. **Correlation method.** It expects you to use some non-repetitive input signal, and finds best shift (lag) with highest cross-correlation value between input and output. Ideally, your DSP should be in a state that doesn't alter the input waveform too much. Moderate compression or distortion seem to not cause any false alarms, though.

Here's an example from one of my projects. Sample rates are hard-coded here, contrary to what I usually suggest these days, but I want to keep it real, so I'm providing this test case as it was, when I was working on that project.

```cpp
HART_TEST ("OutputLimiter - Reports correct latency")
{
    constexpr size_t maxBlockSizeFrames = 1024;

    for (double sampleRateHz : {44.1_kHz, 48_kHz, 88.2_kHz, 96_kHz})
    {
        OutputLimiterHartWrapper outputLimiterHartWrapper;
        outputLimiterHartWrapper.prepare (sampleRateHz, 2, 2, maxBlockSizeFrames);
        const double reportedLatencySeconds = outputLimiterHartWrapper.getLatencySeconds();
        const double expectedLatencySecondsMax = reportedLatencySeconds * 1.01;
        const double expectedLatencySecondsMin = reportedLatencySeconds * 0.99;

        auto outputLimiterHartWrapperReuse = processAudioWith (std::move (outputLimiterHartWrapper))
            .withLabel (HART_STR ("Sample rate at " << sampleRateHz << " Hz, reported latency " << hart::secPrecision << reportedLatencySeconds << " s, correlation method"))
            .withInputSignal (SineSweep() >> GainDb (-12_dB))
            .withBlockSize (maxBlockSizeFrames)
            .withSampleRate (sampleRateHz)
            .withDspPreparation (hart::Preparation::none)
            .inStereo()
            .expectTrue (LatencyBelow (expectedLatencySecondsMax, LatencyBelow::Method::correlation))
            .expectFalse (LatencyBelow (expectedLatencySecondsMin, LatencyBelow::Method::correlation))
            .process();

        processAudioWith (std::move (outputLimiterHartWrapperReuse))
            .withLabel (HART_STR ("Sample rate at " << sampleRateHz << " Hz, reported latency " << hart::secPrecision << reportedLatencySeconds << " s, onset method"))
            .withInputSignal (Impulse() >> GainDb (-12_dB))
            .withBlockSize (maxBlockSizeFrames)
            .withSampleRate (sampleRateHz)
            .withDspPreparation (hart::Preparation::reset)
            .inStereo()
            .expectTrue (LatencyBelow (expectedLatencySecondsMax, LatencyBelow::Method::onset))
            .expectFalse (LatencyBelow (expectedLatencySecondsMin, LatencyBelow::Method::onset))
            .process();
    }
}
```

## Regression (cancellation) tests

A very common type of audio DSP test. Let's say you have already acheived a somewhat desired output sound, and want to re-factor or optimize the DSP code a bit. Sometimes it's pretty scary to do that: what if you break something, but just a little bit, so that the difference is not very obvious? The solution is to capture your current "golden" output of your DSP, then go to town with your DSP optimizations, and compare the output to that pre-rendered reference every now and then.

Other useful scenario is when you (or someone else) have made the DSP algorithm in a different enviroment - say, in Python or Matlab, and now you're porting it to real-time C++. A lot of things can break in this transition, and it you have some reference renders from that research enviroment, it's a good acceptance test that tells you that your C++ implementation is identical.  

```cpp
HART_TEST ("Regression vs some golden output")
{
    HART_REQUIRES_DATA_PATH_ARG;

    processAudioWith (MyDspWrapper())
        .withLabel ("State A")
        .withValue (MyDspWrapper::someParam, someValue)
        .withValue (MyDspWrapper::someOtherParam, someOtherValue)
        .withInputSignal (SineWave (2_kHz))
        .withSampleRate (44100_Hz)
        .withDuration (75_ms)
        .expectTrue (EqualsTo (WavFile ("Golden Output - State A.wav"), 0.001))
        .process();
}
```

This type of tests usually involves reading some wav files from the file system, so `HART_REQUIRES_DATA_PATH_ARG` macro will remind you to pass the path with those wav files to the test binary. It may also be tempting to use wav files as input signal, and you can absolutely do that using `WavFile` signal, but it's encouraged to consider using built-in Signals first. Since you can combine those Signals with built-in (and custom) DSP effects, and use envelopes (for example, to acheive amplitude automation via `GainLinear` or `GainDb`), you can emulate a lot of musical scenarios this way, while reducing i/o use, and keeping only necessary wav files in your repository.

Here, we are using `EqualsTo` matcher, which compares audio frame-by-frame, within a certain tolerance (`0.001` in this case, in linear domain). For less strict checks, you can use ESR - see `hart::esr()` metric.

Also, this test compares audio in the linear domain. It's the most straightforward way to do so, but there's also some use in comparing frequency-domain data, or impulse responses, instead of actual waveforms. As of now, HART doesn't provide a native way to do so, although it probably will at some point in the future.

One of the things that usually adds extra friction with such tests is the fact that you need to generate those "golden" reference files. With HART, there's a separate type of task, created via `HART_GENERATE()` or `HART_GENERATE_WITH_TAGS()` macros:

```cpp
HART_GENERATE ("Envelope - Gain Envelope Regression")
{
    HART_REQUIRES_DATA_PATH_ARG;

    processAudioWith (MyDspWrapper())
        .withLabel ("State A")
        .withValue (MyDspWrapper::someParam, someValue)
        .withValue (MyDspWrapper::someOtherParam, someOtherValue)
        .withInputSignal (SineWave (2_kHz))
        .withSampleRate (44100_Hz)
        .withDuration (75_ms)
        .saveOutputTo ("Gain Envelope A Fail.wav", hart::Save::always, hart::WavFormat::float32)
        .process();
}
```

You can keep them in the same test suite, and even in the same `.cpp` file as your regression test. Running your test binary with `--run-generators` (or `-g`) flag will invoke only those `HART_GENERATE()` tasks, and won't run any `HART_TEST()` ones. And if you run it without this flag, it will do the opposite, so those two types of tasks will never clash, giving you an easy way to update the reference files whenever needed.

## Polarity preserved

Another thing that is easy to miss is when your DSP flips the polarity (phase) of the signal. It's pretty harmless, until the user decides to mix in some dry signal externally in the DAW, and discovers that your plugin doesn't do a very good job at parallel processing. HART provides a matcher that checks exactly that:

```cpp
HART_TEST("Polarity preserved")
{
    const double defaultRenderDuration = hart::CLIConfig::getInstance().getDefaultRenderDurationSeconds();

    processAudioWith (MyDspWrapper())
        .withValue (MyDspWrapper::someParameter, someValue)
        .withLabel (HART_STR ("Some Parameter at " << someValue))
        .withDuration (std::max (defaultRenderDuration, 100_ms))
        .withInputSignal (SineSweep())
        .expectTrue (PolarityPreserved (0.7, 100_ms))
        .process();
}
```

Under the hood, it calculates cross-correlation between input and output signal. In this test case, the max lag time is 100 ms, and if the best correlation within that lag is between 0.7 and 1, the phase is preserved. If it's between -1 and -0.7, then it's flipped. If the correlation is weaker that that, or if the signal is silent, you can set it up its behaviour.

# Effect specific tests

Detailed examples are coming eventually, but here's a few test case titles to give you some ideas:

- Compressor - Does nothing above threshold
- Compressor - Attenuates above threshold
- Compressor - Ratio is correct
- Compressor - Reported gain reduction is correct
- Dry/wet - 100% dry
- Limiter - Doesn't peak above threshold
- Limiter - Reasonable true peaks
- Limiter - Doesn't significantly alter audio below threshold
- Reverb - Has correct tail length
- Reverb - Produces stereo information from mono source
- Saturation - Keeps fundamental frequency at different sample rates
- Saturation - Reduces crest factor
