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
    using enum hart::Unit;  // "dB" unit
    using AudioBuffer = hart::AudioBuffer<float>;
    using hart::crestFactorDb;
    using hart::floatsEqual;

    processAudioWith (MyCompressorDspWrapper())
        .withInputSignal (WavFile ("Snare Single Hit.wav"))
        .inMono()
        .expectTrue (
            [] (const AudioBuffer& output)
            {
                HART_LESS_OR_EQUAL (crestFactor (output).as (dB).get(), 20_dB);
            },
            "Crest Factor - Snare transient is not too sharp"
            )
        .expectTrue (
            [] (const AudioBuffer& output)
            {
                HART_GREATER_OR_EQUAL (crestFactor (output).as (dB).get(), 10_dB);
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
    using enum hart::Unit;  // "linear" unit
    using AudioBuffer = hart::AudioBuffer<float>;
    using hart::crestFactorLinear;

    processAudioWith (MyCompressorDspWrapper())
        .withInputSignal (WavFile ("Snare Single Hit.wav"))
        .inMono()
        .expectTrue (
            [] (const AudioBuffer& input, const AudioBuffer& output)
            {
                return HART_LESS_THAN (
                    crestFactor (output).as (linear).get(),
                    crestFactor (input).as (linear).get())
                    ;
            },
            "Crest Factor - Compressor reduces snare transient"
            )
        .process();
}
```

This is the main idea behind metrics in HART: they give you small re-usable analysis building blocks, so your matcher logic can stay compact and expressive, and highly customizable at the same time.

# Single-channel and multi-channel metrics

Most metrics, such as `crestFactor()`, are fundamentally per-channel properties, i. e. for multi-channel signals, they're calculated individually per each channel. So, if you calculate crest factor on a 5-channel signal, and request to process all the channels (explicitly or implicitly), it will calculate a vector of 5 values, not a scalar. You can also request to calculate the metric only at a specific channel, or a channel subset. For example, to get a value for channel 4 (zero-based) you can do:

```cpp
const double crestFactor = crestFactor (output).ch (4).get();
```
In this case, the crest factor will actually be calculated for only channel 4. Which is useful, as some metrics can be computationally expensive, and we don't want to waste CPU time on the channels we're not interested in.

For mono audio, you can simply do:

```cpp
const double crestFactor = crestFactor (output).get();
```

or, even simpler:

```cpp
const double crestFactor = crestFactor (output);
```

Last form involves implicit conversion to double, which isn't always appropriate (for example, it can confuse the compiler when used with macros like `HART_EXPECT_FLOAT_EQ()`), but use of explicit `.get()` is always safe. Each built-in metric returns a `hart::MetricQuery` object, that you can eventually convert to a scalar value.

The multi-channel version first calculates the metric per channel, and then hands those per-channel values to a reducer (we'll discuss reducers shortly):

```cpp
const double maxCrestFactor = crestFactor (output).get (max());
```

So, if the `output` buffer has two channels, and the per-channel crest factors are `{ 3.2, 5.8 }`, then the `hart::max()` reducer returns `5.8`.

If you want to measure a specific metric only at specific channels, you can list those channels' indices in the optional third argument:

```cpp
const double maxCrestFactor = crestFactorDb (output).ch ({ 0, 3, 1 }).get (max());
```

The order of the per-channel metrics handed to a reducer will obey the order of supplied channel numbers. In this case, if the reducer was `hart::last()`, it would return the crest factor of channel 1, because this channel was listed at the very end of the list.

Some metrics require pairs of channels, for example:

```cpp
const double swappedCorr = maxCrossCorrelation (input, output, 100_ms)
    .ch ({{2, 1}, {3, 0}})
    .get (hart::min());
```

This means the metric will calculate two values:

1. correlation of input, channel 2 vs output, channel 1,
2. correlation of input, channel 3 vs output, channel 0,

and then grab the smallest of two values, as instructed by `hart::min()` reducer (yes, I'm mentioning reducers again - we'll get to them soon). So, you can describe pretty advanced routind this way. For symmetrical pars, like:

1. correlation of input, channel 2 vs output, channel 2,
2. correlation of input, channel 3 vs output, channel 3,

you can do this:

```cpp
const double swappedCorr = maxCrossCorrelation (input, output, 100_ms)
    .ch ({{2, 2}, {3, 3}})
    .get (hart::min());
```

or simply:

```cpp
const double swappedCorr = maxCrossCorrelation (input, output, 100_ms)
    .ch ({2, 3})  // Expands to {{2, 2}, {3, 3}}
    .get (hart::min());
```

So, last form is a shortcut for symmetrical routing. Not to be confused with:

```cpp
const double swappedCorr = maxCrossCorrelation (input, output, 100_ms)
    .ch ({{2, 3}})  // Note the double curly braces
    .get (hart::min());
```

...which results in a single pair - input, channel 2 vs output, channel 3.

In most cases, especially if you're working with mono or stereo channels, you can just skip the `.ch()` call, which will result in a default routing. Default routing is defined by each specific metric, and usually it's the one that makes the most sense for that specific metric.

# What is a reducer?

A reducer is a small callable object that takes a range of values and produces a result from it.

If you're familiar with Python and Pandas, you may think of reducers as something akin to [`DataFrame.groupby()`](https://pandas.pydata.org/docs/reference/groupby.html) aggregations - such as `max`, `mean` or `first`. The idea is very similar: you have multiple measured values, and you want to aggregate them into something useful.

HART ships with a set of reducers such as:

* `hart::first()` - return the first value
* `hart::last()` - return the last value
* `hart::nth()` - return n'th value
* `hart::min()` / `hart::max()` - return the smallest or largest value
* `hart::mean()` / `hart::sum()` - numeric reductions
* `hart::percentile` - return a specific percentile, like median, p99, p95 or any arbitrary number
* `hart::argmin()` / `hart::argmax()` - return the index of the smallest or largest value
* `hart::collect()` - return all values as an `std::vector`
* `hart::anyNaN()` / `hart::allNaN()` - boolean checks over the set of per-channel values
* `hart::allFloatsEqual()` - check whether all values are equal within a tolerance
* `hart::allFloatsEqualToEachOther()` - check whether all values are eqau to each other within a certain tolerance

The result does not have to be a single scalar. While that's the most common case, reducers can also return booleans or containers.

For example:

```cpp
const bool channelsMatch =
    hart::crestFactor (someStereoBuffer).as (dB).get (hart::allFloatsEqual (someToleranceValue));

const std::vector<double> crestFactors =
    hart::crestFactor (someStereoBuffer);.as (dB).get (hart::collect());
```

The first one answers "Are these channels close enough to each other?". The second one gives you the full set of values, in case you want to inspect or post-process them yourself. And you can, of course, make your own reducers, as will be discussed in one of the following sections.

# Do I have to use reducer?

In a lot of cases, you can also omit `get()` entirely, especially for mono signals. In this case, you'll just get the first value in a vector:

``` cpp
// Ok - exlicit reducer, but it's a default one anyways
const double valueA = crestFactor (output).get (first());

// Defaults to first(). Recommended for mono signals.
const double valueB = crestFactor (output).get();

// Implicit conversion to double - often OK
const double valueC = crestFactor (output);

// Explicit case - always ok
const double valueD = static_cast<double> (hart::crestFactor (output));

// C style cast - ok too
const double valueE = (double) crestFactor (output);

// Might confuse compiler, if arguments types are templated
const bool resultA = floatsEqual (crestFactor (output), 1.23);

// OK
const bool resultB = floatsEqual ((double) crestFactor (output), 1.23);

// Even better
const bool resultC = floatsEqual<double> (crestFactor (output), 1.23);

// Will most certainly confuse the compiler
HART_EXPECT_FLOAT_EQ (crestFactor (output), 1.23, 1e-6);

// OK
HART_EXPECT_FLOAT_EQ (crestFactor (output).get(), 1.23, 1e-6);

// Also acceptable
HART_EXPECT_FLOAT_EQ ((double) crestFactor (output), 1.23, 1e-6);
```

Note: Result of running a metric in that manner is typically `double`, but not always. Some metrics may return a different type of value, for example, `int` or `size_t`. And sometimes the result of `.get (...)` can be a `bool` or `size_t` - se the section about the Reducers later in this article.

# Multi-channel metric examples

Let's say your stereo compressor is expected to keep left and right channels reasonably matched in dynamics, when processing a sampled chord:

```cpp
HART_TEST ("Compressor - Stereo channels stay matched")
{
    using AudioBuffer = hart::AudioBuffer<float>;
    using enum hart::Unit;
    constexpr double cfTolerance = 0.25;
    using hart::allFloatsEqual;

    processAudioWith (MyCompressor())
        .withInputSignal (WavFile ("Wide Chord.wav"))
        .inStereo()
        .expectTrue (
            [cfTolerance] (const AudioBuffer& output)
            {
                return HART_TRUE (hart::crestFactor (output).as (dB).get (allFloatsEqual (cfTolerance)));
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
    using enum hart::Unit;
    using hart::max();
    using hart::crestFactor;

    processAudioWith (MyBusProcessor())
        .withInputSignal (WavFile ("Drum Bus.wav"))
        .withInputChannels (4)
        .withOutputChannels (4)
        .expectTrue (
            [] (const AudioBuffer& output)
            {
                return HART_LESS_THAN (crestFactor (output).as (dB).get (max()), 12_dB);
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
            return HART_TRUE (hart::crestFactor (output).as (dB).get (spreadIsBelow1dB));
        },
        "Channel crest factor spread stays under 1 dB"
        )
    .process();
```

This is often enough for very use-case-specific checks. If you find yourself re-using the same reducer in many places, then it may be worth turning it into a named reducer type, similar to the built-in ones.

# Units

A lot of metrics calculate a value that can be represented by different unit. For example, `samplePeak()` can either be in decibels, or in a linear domain, and both units are equally useful. You can request a specific unid from the metric using a chained `ch()` call:

```cpp
//  either:
using enum hart::Unit;  // C++20 and newer

// or:
HART_DECLARE_ALIASES_FOR_UNITS;  // C++17 and earlier

// Default to "Unit::native" unit
const double peakLinearA = samplePeak (buffer);

// Same thing
const double peakLinearB = samplePeak (buffer).as (native);

// This specific metric defaults to "Unit::linear", so also same result
const double peakLinearC = samplePeak (buffer).as (linear);

// This is properly converted to dB
const double peakDb = samplePeak (buffer).as (dB);

// Also okay in most cases
const double peakDb = hart::ratioToDb<double> (samplePeak (buffer));
```

The requested unit is communicated to a specific metric, and each metric knows how to calculate itself in a subset of appropriate units. So, for example, `samplePeak()` knows it calculate decibels using a dB formula for ratio (voltage), and not power, but some other metric can decide to use power formula instead. You won't have to worry about it, but if you do, you can always refer to each metric's documentation.

Converting to a specific unit happens before the reducer, for example:

```cpp
const double valueA = samplePeak (multiChannelBuffer).as (dB).get (mean());
const double valueB = ratioToDb<double> (samplePeak (multiChannelBuffer).as (linear).get (mean()));
HART_EXPECT_FLOAT_NE (valueA, valueB, 1e-6);
```

Unless all channels peak at the same value, `valueA` and `ValueB` will be different. In first case, values are converted to dB for each channel, and then mean value is calculated. If latter case, mean value is calculated in linear domain, and only then converted to dB. Mathematically, those are different types of averaging, and you have means to perform either of those.

# When to use metrics

Metrics are especially handy when:

* you need a custom check that is too specific for stock matchers
* you want to compare input and output in a readable and concise way
* you want to express a matcher in terms of some measurable property, that is already represented by one of the built-in metrics

If your check is simple and likely to be re-used across a few test cases, you might want to turn it into a dedicated custom Matcher, possibly with your custom implementation of the metric'a math under the hood. But for many practical cases, a metric plus a short lambda-based matcher is already the sweet spot.

# What's next?

Metrics are just one way to write custom checks. To learn more about using function-based matchers and custom DSP wrappers in general, see @ref TestingYourDspInHart.
