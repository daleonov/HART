@page ParametrisedTests Parametrised Tests

# What are parametrised tests for?

Parametrised tests let you run the same test case with different values, usually meaning different setups. Let's say you have a test case like this one:

```cpp
HART_TEST ("Always retains input sample peaks")
{
    processAudioWith (MyDSP())
        .withInputSignal (SineWave() >> GainDb (-3_dB))
        .expectTrue (PeaksAt (-3_dB))
        .process();
}
```

This states that the output sample peaks should always stay at the same level as the input. But one arbitrary case does not really prove "always", right? You may want to check a few different input levels and a few different sine frequencies, to make sure the behaviour holds across a wider range of conditions.

One way to do it is to use loops:

```cpp
HART_TEST ("Always retains input sample peaks")
{
    for (const double inputLevelDb : {-60_dB, -12_dB, -3_dB, 0_dB, +9_dB})
    {
        for (const double sineFrequencyHz : {60_Hz, 1_kHz, 15_kHz})
        {
            processAudioWith (MyDSP())
                .withInputSignal (SineWave (sineFrequencyHz) >> GainDb (inputLevelDb))
                .expectTrue (PeaksAt (inputLevelDb))
                .process();
        }
    }
}
```

This works, but the test case is more complex now. A good TDD practice is to keep cyclomatic complexity in tests at 1, whenever possible. Unlike production code, tests should usually be "correct on inspection": you should be able to read them quickly and understand exactly what behaviour they specify.

In HART, you can express the same test as a parametrised test like this one:

```cpp
HART_PARAMETRISED_TEST ("Always retains input sample peaks")
{
    const double inputLevelDb = HART_GENERATE_VALUE (-60_dB, -12_dB, -3_dB, 0_dB, +9_dB);
    const double sineFrequencyHz = HART_GENERATE_VALUE (60_Hz, 1_kHz, 15_kHz);

    processAudioWith (MyDSP())
        .withInputSignal (SineWave (sineFrequencyHz) >> GainDb (inputLevelDb))
        .expectTrue (PeaksAt (inputLevelDb))
        .process();
}
```

Notice the `HART_PARAMETRISED_TEST()` declaration and the two `HART_GENERATE_VALUE()` statements inside. Unlike regular test cases declared with `HART_TEST()`, HART will run this test body multiple times, each time with a new combination of generated values.

In other words, this behaves like the two nested loops above, but the test body stays flat. That is the main benefit: the values that define the test space are visible at the top, and the actual behaviour under test remains easy to read.

# Value combinations

If you are familiar with the [Catch2 framework](https://catch2.org), `HART_GENERATE_VALUE()` is similar to Catch2's `GENERATE()` statement. Multiple `HART_GENERATE_VALUE()` statements produce a Cartesian product of their value sets. The test case above will run exactly `5 * 3 = 15` times.

Each generated combination is a separate invocation of the test body. This gives you a cleaner mental model than manual loops: every permutation starts from the top of the test case, with fresh local variables and the usual HART setup flow.

You can pass explicit sequence of values of any type:

```cpp
const double inputLevelDb = HART_GENERATE_VALUE (-60_dB, -12_dB, -3_dB, 0_dB, +9_dB);
```

You can also use common C++ containers:

```cpp
// Or std::vector, std::set, or other STL and STL-like containers
const std::array<double, 5> inputLevelsDb = {{ -60_dB, -12_dB, -3_dB, 0_dB, +9_dB }};

// Using a pair of iterators
const double inputLevelDb = HART_GENERATE_VALUE (inputLevelsDb.begin(), inputLevelsDb.end());

// ...or just a reference to the full container
const double inputLevelDb = HART_GENERATE_VALUE (inputLevelsDb);
```

When using unordered containers, remember that HART follows the container's iteration order. If stable run order matters to you, prefer `std::array`, `std::vector`, `std::set`, or another container with predictable iteration order.

# Where to use generated values

If you use `HART_GENERATE_VALUE()` inside a test case, declare that test case with `HART_PARAMETRISED_TEST()` instead of `HART_TEST()`. The test runner treats parametrised tasks differently. If you use tags in your test suite, you can use `HART_PARAMETRISED_TEST_WITH_TAGS()`.

Keep `HART_GENERATE_VALUE()` statements at the top level of the test case body. Avoid putting them inside `if` statements, loops, or branches whose execution depends on another generated value. HART expects the same generated-value statements to be reached in the same order on every permutation.

Generated value expressions should also be cheap and repeatable. HART may rebuild the value sequence while running permutations, so avoid expressions with important side effects, such as network calls or writes to external state, inside `HART_GENERATE_VALUE()`.

# Capturing values

When a parametrised test fails, you usually want to know which value combination has failed. You can use `HART_CAPTURE_VALUE()` for that:

```cpp
HART_PARAMETRISED_TEST ("Always retains input sample peaks")
{
    const double inputLevelDb = HART_GENERATE_VALUE (-60_dB, -12_dB, -3_dB, 0_dB, +9_dB);
    const double sineFrequencyHz = HART_GENERATE_VALUE (60_Hz, 1_kHz, 15_kHz);

    HART_CAPTURE_VALUE (inputLevelDb);
    HART_CAPTURE_VALUE (sineFrequencyHz);

    processAudioWith (MyDSP())
        .withInputSignal (SineWave (sineFrequencyHz) >> GainDb (inputLevelDb))
        .expectTrue (PeaksAt (inputLevelDb))
        .process();
}
```

If the test fails, HART will print the captured values as part of the failure report, for example:

```text
Captured values:
inputLevelDb = -12
sineFrequencyHz = 1000
```

`HART_CAPTURE_VALUE()` is similar to Catch2's `CAPTURE()` macro. It is not limited to parametrised tests: you can use it in regular `HART_TEST()` cases as well, for example to capture the current value in a manual loop or an intermediate measurement before an assertion.

# Why not just use loops?

Loops are still fine when they are genuinely part of the behaviour you are testing. Parametrised tests are more appropriate when the loop exists only to repeat the same assertion over a set of independent input values.

Compared with manual loops, parametrised tests usually give you:

- A flatter, more readable test body
- A clearer list of input values at the top of the test
- A direct Cartesian-product model when several dimensions are tested together
- Separate test runner invocations for each value combination

The practical result is that broad coverage does not have to make the test harder to read.
