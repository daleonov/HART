@page UsingMetricsAndReducers Using Metrics And Reducers

@tableofcontents

# What are metrics for?

Metrics are built-in functions that help you measure some common audio properties, and express custom checks, based on them, in a short and readable way.

They're mostly meant to be used when writing your own Matchers, especially light-weight function-based ones (see `hart::MatcherFunction`), although you may also use the same metric functions inside a full custom `hart::Matcher` subclass as well.

For example, if you want to check something like:

* Does this DSP effect reduce the crest factor of the signal?
* Are all output channels correlated closely enough to each other?
* Is the left channel's RMS above -10 dB?

...a simple function-based matcher using metrics is often the cleanest way to express it.

# Metrics inside function-based matchers

If you're making a quick custom check, it is often enough to pass a lambda to `expectTrue()` or `assertTrue()`, or their inverted counterparts. HART supports function-based matchers with these signatures:

1. `bool matcherFunction (const AudioBuffer<SampleType>& output)`
2. `bool matcherFunction (const AudioBuffer<SampleType>& input, const AudioBuffer<SampleType>& output)`

Using built-in metrics in both of those forms will let you express some non-trivial matchers in a very human-readable form. For more details on making use of such matchers, refer to `MatcherFunction` and `AudioTestBuilder` documentation. Also, check respective sections in @ref TestingYourDspInHart.

## Example - Checking the impact of your DSP on signal's dynamic range

Let's say you have a compressor, and you expect it to control the drum sample's dynamic range. You might want to express that the processed drum clip doesn't have a very sharp transient, but is not completely "slammed" at the same time. [Crest factor](https://en.wikipedia.org/wiki/Crest_factor) is quite an appropriate metric to express that.

```cpp
HART_TEST ("MyCompressor - Crest factor is in reasonable range")
{
    using AudioBuffer = hart::AudioBuffer<float>;
    using hart::crestFactorDb;
    using hart::floatsEqual;

    processAudioWith (MyCompressorDspWrapper())
        .withInputSignal (WavFile ("Snare Single Hit.wav"))
        .inMono()
        .expectTrue (
            [] (const AudioBuffer& output)
            {
                return crestFactorDb (output) <= 20_dB;
            },
            "Crest Factor - Snare transient is not too sharp"
            )
        .expectTrue (
            [] (const AudioBuffer& output)
            {
                return crestFactorDb (output) >= 10_dB;
            },
            "Crest Factor - Snare transient is not too slammed"
            )
        .process();
}
```

Or perhaps you don't have a specific crest factor value in mind, and merely want to express that your compressor reduces crest factor, as compared to its input:

```cpp
HART_TEST ("MyCompressor - Reduces crest factor of the signal")
{
    using AudioBuffer = hart::AudioBuffer<float>;
    using hart::crestFactorLinear;

    processAudioWith (MyCompressorDspWrapper())
        .withInputSignal (WavFile ("Snare Single Hit.wav"))
        .inMono()
        .expectTrue (
            [] (const AudioBuffer& input, const AudioBuffer& output)
            {
                return crestFactorLinear (output) < crestFactorLinear (input);
            },
            "Crest Factor - Compressor reduces snare transient"
            )
        .process();
}
```

This is the main idea behind metrics in HART: they give you small re-usable analysis building blocks, so your matcher logic can stay compact and expressive, and highly customizable at the same time.

# Single-channel and multi-channel metrics

Some metrics, such as `crestFactor()`, are fundamentally per-channel properties. In HART, such metrics would usually come in two forms:

1. A single-channel version
2. A multi-channel version that takes a reducer

The single-channel version gives you a scalar for one channel:

```cpp
const double crestFactor = hart::crestFactorDb (output, hart::Channel::right);
```

Or, for mono audio, simply:

```cpp
const double crestFactor = hart::crestFactorDb (output);
```

The multi-channel version first calculates the metric per channel, and then hands those per-channel values to a reducer (we'll discuss reducers shortly):

```cpp
const double maxCrestFactor = hart::crestFactorDb (hart::max(), output);
```

So, if the `output` buffer has two channels, and the per-channel crest factors are `{ 3.2, 5.8 }`, then the `hart::max()` reducer returns `5.8`.

If you want to measure a specific metric only at specific channels, you can list those channels' indices in the optional third argument:

```cpp
const double maxCrestFactor = hart::crestFactorDb (hart::max(), output, { 0, 3, 1 });
```

The order of the per-channel metrics handed to a reducer will obey the order of supplied channel numbers. In this case, if the reducer was `hart::last()`, it would return the crest factor of channel 1, because this channel was listed at the very end of the list.

# What is a reducer?

A reducer is a small callable object that takes a range of values and produces a result from it.

If you're familiar with Python and Pandas, you may think of reducers as something akin to [`DataFrame.groupby()`](https://pandas.pydata.org/docs/reference/groupby.html) aggregations - such as `max`, `mean` or `first`. The idea is very similar: you have multiple measured values, and you want to aggregate them into something useful.

HART ships with a set of reducers such as:

* `hart::first()` - return the first value
* `hart::last()` - return the last value
* `hart::min()` / `hart::max()` - return the smallest or largest value
* `hart::mean()` / `hart::sum()` - numeric reductions
* `hart::argmin()` / `hart::argmax()` - return the index of the smallest or largest value
* `hart::collect()` - return all values as an `std::vector`
* `hart::anyNaN()` / `hart::allNaN()` - boolean checks over the set of per-channel values
* `hart::allFloatsEqual()` - check whether all values are equal within a tolerance

The result does not have to be a single scalar. While that's the most common case, reducers can also return booleans or containers.

For example:

```cpp
const bool channelsMatch =
    hart::crestFactorDb (hart::allFloatsEqual (someToleranceValue), someStereoBuffer);

const std::vector<double> crestFactors =
    hart::crestFactorDb (hart::collect(), someStereoBuffer);
```

The first one answers "Are these channels close enough to each other?". The second one gives you the full set of values, in case you want to inspect or post-process them yourself. And you can, of course, make your own reducers, as will be discussed in one of the following sections.

# Multi-channel metric examples

Let's say your stereo compressor is expected to keep left and right channels reasonably matched in dynamics, when processing a sampled chord:

```cpp
HART_TEST ("Compressor - Stereo channels stay matched")
{
    using AudioBuffer = hart::AudioBuffer<float>;
    constexpr double cfTolerance = 0.25;

    processAudioWith (MyCompressor())
        .withInputSignal (WavFile ("Wide Chord.wav"))
        .inStereo()
        .expectTrue (
            [=] (const AudioBuffer& output)
            {
                return hart::crestFactorDb (hart::allFloatsEqual (cfTolerance), output);
            },
            "Left and right crest factor stay close"
            )
        .process();
}
```

Or perhaps you want to make sure that none of the channels in a drum bus exceed some crest factor limit:

```cpp
HART_TEST ("Drum bus - Crest factor upper bound")
{
    using AudioBuffer = hart::AudioBuffer<float>;

    processAudioWith (MyBusProcessor())
        .withInputSignal (WavFile ("Drum Bus.wav"))
        .withInputChannels (4)
        .withOutputChannels (4)
        .expectTrue (
            [] (const AudioBuffer& output)
            {
                return hart::crestFactorDb (hart::max(), output) < 12_dB;
            },
            "No channel exceeds 12 dB crest factor"
            )
        .process();
}
```

# Making your own reducer

You are not limited to the stock reducers. Any callable that accepts two iterators and returns something useful can be used as a reducer.

For instance, perhaps you want to know whether the spread between the smallest and largest crest factors stays below 1 dB:

```cpp
const auto spreadIsBelow1dB =
    [] (AudioBuffer begin, AudioBuffer end)
    {
        if (begin == end)
            return true;

        const auto minmax = std::minmax_element (begin, end);
        return (*minmax.second - *minmax.first) < 1.0;
    };
```

Then use it like this:

```cpp
processAudioWith (MyCompressor())
    .withInputSignal (WavFile ("Drum Overheads.wav"))
    .inStereo()
    .expectTrue (
        [&] (const AudioBuffer& output)
        {
            return hart::crestFactorDb (spreadIsBelow1dB, output) == true;
        },
        "Channel crest factor spread stays under 1 dB"
        )
    .process();
```

This is often enough for very use-case-specific checks. If you find yourself re-using the same reducer in many places, then it may be worth turning it into a named reducer type, similar to the built-in ones.

# When to use metrics

Metrics are especially handy when:

* you need a custom check that is too specific for stock matchers
* you want to compare input and output in a readable and concise way
* you want to express a matcher in terms of some measurable property, that is already represented by one of the built-in metrics

If your check is simple and likely to be re-used across a few test cases, you might want to turn it into a dedicated custom Matcher, possibly with your custom implementation of the metric'a math under the hood. But for many practical cases, a metric plus a short lambda-based matcher is already the sweet spot.

# What's next?

Metrics are just one way to write custom checks. To learn more about using function-based matchers and custom DSP wrappers in general, see @ref TestingYourDspInHart.
