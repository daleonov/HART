@page TestingYourDspInHart Testing Your DSP in HART

## Wrapping your algorithm

In order to get HART to play audio through your DSP algorithm, you need to wrap in into a DSP class. @ref DSP @ref hart::DSP

To do it, make a subclass of of DSP and put your class inside of it. You'll have to implement a few methods for it. A minimal setup can look like this:

```cpp
class MyDSPWrapper :
    public hart::DSP<float>
{
public:
    // Optional, but encouraged to do - for setValue()
    enum Params
    {
        someParamID,
        someOtherParamID
    };

    MyDSPWrapper();
    // Also, move and copy ctros and assignements are optional, but encouraged

    void prepare (
        double sampleRateHz,
        size_t numInputChannels,
        size_t numOutputChannels,
        size_t maxBlockSizeFrames
        ) override;

    void process (
        const AudioBuffer<SampleType>& input,
        AudioBuffer<SampleType>& output,
        const EnvelopeBuffers& envelopeBuffers
        ) override;

    void reset() override;
    void setValue (int id, double value) override;
    void represent (std::ostream& stream) const override;
    bool supportsEnvelopeFor (int id) const override;
    bool supportsChannelLayout (size_t numInputChannels, size_t numOutputChannels);
    virtual bool supportsSampleRate (double sampleRateHz) const;

    // Or HART_DSP_FORBID_COPY_AND_MOVE if your class not copyable/movable
    HART_DSP_DEFINE_COPY_AND_MOVE (MyDSPWrapper);

private:
    MyActualEffectClass& m_effect;
};

```

It may look like a lot, but you probably already have similar methods implemented in your effect, so it won't take too much work. Check the hart::DSP reference for details about each of those methods.

HART supports processing in both ```float``` and ```double``` when it comes to audio data - everything that has to do with audio buffers is templated. Everything that has to do with parameters (like gains, compressor thresholds etc) is always ```double``` - keeps thigs way simpler! 

## Your main file

Like most automated tests, they're designed to run as a separate executable. So, if you're making an audio plugin or something like that, just make a separate target (project) that builds into a basic console application. Your main function should something look like this:

```cpp
#define HART_IMPLEMENTATION  // It's a header-only library, so this is required
#include "hart.hpp"  // Just one header

int main (int argc, char** argv)
{
    return HART_RUN_ALL_TESTS (argc, argv);
}
```

This is it!

## Your first test

In your main cpp, or a separate cpp file, declare a test function like this:

```cpp
#include "hart.hpp"

HART_TEST ("My first HART Test")
{
}
```

Nod build and run it! You can also use \ref HART_TEST_WITH_TAGS if you want to use tags, like in [Catch2](https://github.com/catchorg/Catch2)! Now let's do something simple:

```cpp
#include "hart.hpp"

HART_TEST ("My first HART Test")
{
    processAudioWith (MyDSPWrapper())  // [1]
        .withInputSignal (SineWave())  // [2]
        .withValue (MyDSPWrapper::someParamID, 6.7)  // [3]
        .expectTrue (PeaksAt (-3_dB))  // [4]
        .process();  // [5]
}
```

It's probably pretty clear what this tests is trying to express, but let's break it down.

### [1] Instantiating your effect

You create your wrapped effect's instance and hand it over to the test host (test runner). There are a few ways to do it, but in any case it will own and instance of your effect. You create, run the test, and it's gone. For the next test, you make another instance of the effect. I might relax this rule and let you pass a non-owned pointer to it, if enought people as me to, but right now I think this is the way to do it.

If your effect doesn't have copy/move semantics, you can still pass it wrapped in a smart pointer like so:

```cpp
processAudioWith (std::make_unique<MyDSPWrapper>())
```

So if your object is not trivially movable or copyable, you can still use HART for testing it with.

### [2] Defining input signal

You feed some audio into your effect, to check what comes out. This is the core purpose of this framework. In the future, I'll probably add support for synths and virtual instruments - I need that too - but audio effects is a priority.

"Signal" is one of the core concepts of this framework. It  can be as simple as a sine wave. It can be something more complex: with a chain of effects with automation envelopes to shape it. Or it can be just a wav file - I know a lot of people just want to play some pre-rendered audio through it, you can do it too, easily!

### [3] Setting some values

Obviously, you want to put your effect in some state first, in most cases. In most cases, you just want to set a few fixed values - just chain a few withValue() statements - they will call DSP::setValue() that you've implemented earlier. Of course, you can not do those statements at all, and it will keep your effect in its default state.

You can also do something more fance with the parameters - think automation curves like in DAWs, or the LFOs. You can do this to in HART - more on this later! Check Envelopes section of this reference for details.

### [4] Check the audio produced by your effect

This is what this framefork is for, after all. PeaksAt is something called "matcher" (totally stole this term from Catch2). If receives audio from your effect's output and checks it. This one, as the name implies, checks if the signal peaks at 3dB. By the way, you get a bunch of handy constants and literalls for better readability, like ```_dB``` or ```_kHz``` - you're welcome to use them.

Other matchers can, for example, compare your output with some other signal or a wav file. And they're passed in as objects - it means you make your own and use them, just like the stock ones. So, if you need, say, check LUFS values, inter-sample peaks, or check something fancy in frequency domain, just subclass a Matcher, and pass it to the test runner.

You can do two levels of assertions: "expect" and "assert". Like in other test frameworks, "assert" will stop the test immediately if it fails, but "expect" will report the failure and carry on with other tests. And there are inverted versions for both, so you get:
    * ```expectTrue()```
    * ```expectFalse()```
    * ```assertTrue()```
    * ```assertFalse()```

You can have as many assertions/expectations in a single test - just keep chaining them together. They will be checked in that order, whenever possible. Hovewer, some matchers need to wait for the full signal to be generated (like ```PeaksAt```), while others can work on block-by-block basis (like ```EqualsTo```), so the order is not guaranteed.

You also have ```HART_ASSERT_TRUE()``` and ```HART_EXPECT_TRUE()``` for trivial non-audio checks, in case you need them. But you shoudn't use HART for testing everything - use it for audio tests, and stick with Google Test (gtest) or Catch2 for everything else.

See @ref hart::AudioTestBuilder for the full list of options you can set.

### [5] Run the test

You should always call it after your setup steps. Everything between steps [1] and [5] can come in any order, these are just some lightweight set up calls. Calling process launches the test, processes audio block by block and runs the checks. If any of these fails, you'll get a readable description of what went wrong.

## Setting up the audio

What about the sample rate? Or the block size? I'm glad you asked! We didn't set those up earlier, because we were using a default configuration, which is:

Parameter       | Value
----------------|-------
Sample Rate     | 44100 Hz
Block Size      | 1024 frames
Duration        | 100 ms
Input Channels  | 1 (mono)
Output Channels | 1 (mono)

If you're looking for something different, you can change those like so:

```cpp
hart::processAudioWith (testedBoosterProcessor)
    .withSampleRate (48_kHz)
    .withBlockSize (64)
    .withDuration (325.5_ms)
    .withInputChannels (5)
    .withOutputChannels (10)

    // Set your input signal
    // Set effect's values
    // Set your assertions

    .process();
```

For channel configurations, you can use some handy aliases:
 * ```inMono()``` - sets both input and output to mono
 * ```inStereo()``` - sets both input and output to stereo
 * ```withStereoInput()```
 * ```withStereoOutput()```
 * ```withMonoInput()```
 * ```withMonoOutput()```

You can skip any parameters that don't care about (keeping them at their default values), and only set the specific ones. For more details, check @ref AudioTestBuilder methods.

## Logging the audio

If your test fails, you might want to check what was the input or output audio. You can tell HART to output the audio easily:

```cpp
processAudioWith (MyDSPWrapper())
    // Set up everything
    .saveInputTo ("my_failed_test_input.wav")
    .saveOutputTo ("my_failed_test_output.wav")
    .process();
```

By default, they will save the audio only when any of the checks fail, but you can tell them to save audio regardless of the result via the second argument - handy for generating data for regression tests. Supported formats are PCM at 16, 24 and 32 bits and float at 32 bit. Default is PCM24. You can use absolute or relative paths. For relative paths, set the ```--data-root-path``` CLI parameter to wherever you want HART to save them.

## Playing the pre-rendered audio

I assume most people just want to pre-render some audio and play it through your effect. While it's absolutely possible with HART, I encourage you to explore signal generation with HART before you fall on your old habit of using pre-rendered wav's. But here's how you do it:

```cpp
processAudioWith (MyDSPWrapper())
    .withInputSignal (WavFile ("my_test_input.wav"))
    // Your other set up
    .expectTrue (EqualsTo (WavFile ("my_test_reference_output.wav")))
    .process();
```

Done! See @ref Signals::WavFile for the details. You might also want to put the @ref HART_REQUIRES_DATA_PATH_ARG macro at the beginning of test cases that use relative path - it will remind you to pass the respective CLI argument of you forgot to do so.

## Generating test signals

HART is designed to create complex signals by expressing them with the code. This way you can avoid fumbling with test generators in your DAW and hoarding a ton of wav files as your input test signals.

You've already seen a few of the signals - ```Silence```, ```SineWave``` and ```WavFile```. There's more of those, of course, like ```SineSweep``` or ```WhiteNoise```, and more will come in the future. But what's even better is that you can shape them before feeding them into your effect, or before comparing your effect's output to them.

First, you can add effects to them. For example, if you want to have a ```SineWave``` at -3dB, you can do it like so: ```SineWave() >> GainDb (-3_dB)```. Let's actually do something more complex:

```cpp
processAudioWith (MyDSPWrapper())
    .withInputSignal (SineWave (3.5_kHz, halfPi) >> GainDb (+2.5_dB) >> HardClip (-3_dB))
    // ...
    .process();

```

At first we have a sine wave at 3.5 kHz, with starting phase at &pi;/2 radians. By the way, all the previous one were jusr created with default frequency, which is 1 kHz. It always outputs the signal at 0 dB sample peak level, and, like most other signals, you cannot set its level in constructor - to do so, you just throw a gain effect after it. Which is exactly what is happening here. ```GainDb``` is one of the DSP effects built into HART framework. By the way, it's also a ```hart::DSP``` subclass just like MyDSPWrapper we've just defined, so you can use them interchangeably! Anf after that, it gets clipped at -3 dB, turning it into a somewhat square-ish shape.

If you're curious what's going on behind the fancy syntax: hart::Signal objects can store a sequence of DSP effects inside of them. When ```process()``` is called, the y initialize the whole chain, generate audio, and play it through their DSP
chain, at whatever sample rate, block size et cetera you're set your audio test to. Signal is the host here, it owns, runs and manages those effect instances. Effects can not be attached to each other - they need some Signal source to own them. And Signal can be a lot of things, like a WavFile, for instance. The order of the effects is guaranteed to be preserved: whatever gets added first, receives the audio first.

Signals can take any DSP instances, including your own effect. So those two pieces of code produce the same audio:

```cpp
// [1]
processAudioWith (MyDSPWrapper())
    .withInputSignal (SineWave())
    // ...
    .process();

// [2]
processAudioWith (GainDb (0_dB))
    .withInputSignal (SineWave() >> MyDSPWrapper())
    // ...
    .process();
```

Althouth [1] is, of course, a prefferred approach, and it gives you an easier interface to set up your effect's values. But you have multiple DSP algorithms to test, you can easily chain them together in any order, taking some inspiration form example [2].

And you can also make your own little utility DSP classes to shape the signals - at this point, you already know how to subclass hart::DSP.

## Parameter automation envelopes

But wait, there's more! Remember when we set sine gave's level to 2.5 dB in the previuos chapter? It was a fixed value. We can change in time. Let's say, we want the gain to start at -3 dB, but then after 10 ms jump to -10 dB, stay there for 50 ms, and then slowly crawl to -1 dB in an s-curved manner for 100 ms.

Here's how you do it. To express this curve, you can do something like this:

```cpp
const auto myGainEnvelope = SegmentedEnvelope (decibelsToRatio (-3_dB))
    .hold (10_ms)
    .rampTo (decibelsToRatio (-10_dB), 5_ms)
    .hold (50_ms)
    .rampTo (decibelsToRatio (-1_dB), 100_ms, SegmentedEnvelope::Shape::sCurve);
```

Notice how it's not attached to any DSP unit or host yet, it's just a lightweight object that stores some data about what ho some value should change in time. It doesn't even have to know anything about the effect, signal, or your audio test set up like sample rate or channel number. It supports a few different shapes of ramp transitions, like linear, exponential or s-curve.

And now, you can attach it to your (or any other) effect:

```cpp
// [1] - Just a fixed value
const myEffect1 = MyDSPWrapper().withValue (MyDSPWrapper::someParamID, 2.5_dB);

// [2] - Same parameter, but changes in time
const myEffect1 = MyDSPWrapper().withEnvelope (MyDSPWrapper::someParamID, myGainEnvelope);
```

Now pause and try to apply it as Gain for a SineWave. Note: if you're doing slow gain ramps and want a specific curve, you might want to use GainLinear effect instead of GainDB. Applying a linear curve to a value in decibels is awkward, so you won't get a proper linear curve with GainDb. But if you don't care about it, you can just use either of those.

Now, if you want to feed the envelopes into your own processor, you probably need to know how to support them properly. First, the host of this DSP will figure out the value rendering part, you won't have to worry about it. In each ```process()``` callback you'll get a container with envelope curves together with your audio buffers. It's a hash map - key is your parameter's id, and value is a container with pre-rendered values for this parameter, same length as audio buffers. Did I mention it's a sample accurate autometion? It's a sample accurate automation! So you can fetch it an use it like so:

```cpp
void process (const AudioBuffer<SampleType>& input, AudioBuffer<SampleType>& output, const EnvelopeBuffers& envelopeBuffers) override
{
    const bool hasGainEnvelope = envelopeBuffers.contains (someParamID); // [1]

    if (hasGainEnvelope)
    {
        std::vector<double>& someParamEnvelopeValues = envelopeBuffers[someParamID];
        const double someLazyParamValue = someParamEnvelopeValues[0];  // [2]

        for (size_t channel = 0; channel < input.getNumChannels(); ++channel)
        {
            for (size_t frame = 0; frame < input.getNumFrames(); ++frame)
            {
                const double someParamValue = someParamEnvelopeValues[frame]; // [3]
                // Do something with it
                // ...
            }
        }
    }

    else
    {
        const double someParamValue = m_myFixedValue;
        // Render as a fixed value
        // ...
    }
}

```

If there's no envelope attached to your DSP, ```envelopeBuffers``` will have no record of, so you can check it like in [1]. You can treat it like a block-accurate automation and grab just one value (first one like in [2], or mean, max or whatever), like it's typically done in most audio plugins. Or you can use it properly like in [3], potentially having a different param value for every frame (sample) of audio. Obviously, if you're merely imlementing ```process()``` for testing your effect, you must mirror what your underlying effect already does.

There's also a ```supportsEnvelopeFor()``` callback that will get triggered by the host, you can return false for the parameter ids that you don't want to support envelopes for, and you won't get ```envelopeBuffers``` for those.

## LFOs

It's also possible to use @ref hart::Signal and an envelope parameter. For example, automating gain with a SineWave, like an LFO. It's not implemented yet, but if you want to beat me to it, just subclass hart::Envelope and make your own!

## Generating audio for regression and acceptance tests

Obviuosly, if you want to compare your effect's output to pre-recorded wav's, you need those wav files first. You can do it with just regular test cases, of course, but HART has special ones just for this. Use @ref HART_GENERATE() or @ref HART_GENERATE_WITH_TAGS() instead of usual tests. Under the hood, they're pretty much the same as regular test cases, but will help to keep "test" and "generate" tasks separate, of you choose to do them in the same target (project).

To run tasks defined with those macros run your HART test binary with a ```--run-generators``` (or ```-g```) flag. It will skip all tests and run the generators. Without this flag, it will run only tests, and skip the generators.

## Command line interface

If you run your test binary with a ```--help``` CLI argument, it will tell you everything you need to know. Things you can do with it:

* Set data root path for your relative file paths (like wav files)

* Set random seed for everything random. By the way, everything random is guaranteed to be deterministin in HART!

* Set number of decimal points for various values (like decibels, seconds etc)

* Choose to run just tests or just generators

* Ask HART to shuffle your test cases

Someday there will be option for tags and threaded runs as well. Hopefully!

## Some more test examples

Check ```tests``` directory for the examples. Is there a better way to document an automated test framework, than to read the tests used by it to test itself?
