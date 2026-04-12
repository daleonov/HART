#include <memory>
#include "hart.hpp"

HART_DECLARE_ALIASES_FOR_FLOAT;
using hart::decibelsToRatio;
using hart::floatsEqual;
using hart::ResetSignal;

HART_TEST ("Host - DSP Move, Copy and Transfer")
{
    // This doesn't really test much, but it acts as an
    // example on how you can pass your DSP to the host.

    // 1. Move:
    GainDb moveMe;
    processAudioWith (std::move (moveMe))
        .withInputSignal (SineWave())
        .expectTrue (EqualsTo (SineWave()))
        .process();

    // ...but you should usually just do this:
    processAudioWith (GainDb())
        .withInputSignal (SineWave())
        .expectTrue (EqualsTo (SineWave()))
        .process();

    // 2. Copy:
    // Must be copied explicitly via copy() call
    const GainDb copyMe;
    processAudioWith (copyMe.copy())
        .withInputSignal (SineWave())
        .expectTrue (EqualsTo (SineWave()))
        .process();

    // ...and of course it should be reusable after copying:
    processAudioWith (copyMe.copy())
        .withInputSignal (SineWave())
        .expectTrue (EqualsTo (SineWave()))
        .process();

    // 3. Transfer ownership, if implementing copy/move semantics is not an option:
    auto ownMe = hart::make_unique<GainDb>();
    processAudioWith (std::move (ownMe))
        .withInputSignal (SineWave())
        .expectTrue (EqualsTo (SineWave()))
        .process();

    // ...or like so:
    processAudioWith (hart::make_unique<GainDb>())
        .withInputSignal (SineWave())
        .expectTrue (EqualsTo (SineWave()))
        .process();
}

HART_TEST ("Host - DSP Re-use")
{
    // process() will spit out the DSP after processing...
    auto reuseMe = processAudioWith (GainDb (-3_dB))
        .withInputSignal (SineWave())
        .expectTrue (PeaksAt (-3_dB))
        .process();

    /// ...so you can re-use it
    reuseMe = processAudioWith (std::move (reuseMe))
        .withInputSignal (SineWave())
        .expectTrue (PeaksAt (-3_dB))
        .process();
}

HART_TEST ("Signal Chain - DSP Move, Copy and Transfer")
{
    GainDb moveMe;
    const GainDb copyMe;  // Must be copied explicitly via copy() call
    std::unique_ptr<hart::DSPBase<float>> ownMeAsAbstract = hart::make_unique<GainDb>();
    std::unique_ptr<GainDb> ownMeAsDerived = hart::make_unique<GainDb>();

    processAudioWith (GainDb (0_dB))
        .withInputSignal (SineWave() >> std::move (moveMe) >> copyMe.copy() >> std::move (ownMeAsAbstract) >> std::move (ownMeAsDerived))
        .process();
}

HART_TEST ("EqualsTo - Signal Move, Copy and Transfer")
{
    const auto copyMe = SineWave();
    auto moveMe = SineWave();
    auto transferMe = hart::make_unique<SineWave>();

    processAudioWith (GainDb (0_dB))
        .withInputSignal (SineWave())
        .expectTrue (EqualsTo (copyMe))
        .expectTrue (EqualsTo (std::move (moveMe)))
        .expectTrue (EqualsTo (std::move (transferMe)))
        .process();
}

HART_TEST ("Move audio output to an external buffer")
{
    hart::AudioBuffer<float> bufferA;
    HART_ASSERT_TRUE (hart::floatsEqual (bufferA.getLengthSeconds(), 0.0));

    processAudioWith (GainDb (0_dB))
        .withLabel ("Saving via buffer reference")
        .withInputSignal (SineWave())
        .withDuration (1_ms)
        .saveOutputTo (bufferA)
        .process();

    HART_EXPECT_TRUE (hart::floatsEqual (bufferA.getLengthSeconds(), 1_ms, 5_us));

    hart::AudioBuffer<float> bufferB;
    bool wasCalled = false;
    HART_ASSERT_TRUE (hart::floatsEqual (bufferB.getLengthSeconds(), 0.0));

    processAudioWith (GainDb (0_dB))
        .withLabel ("Saving via callable")
        .withInputSignal (SineWave())
        .withDuration (1_ms)
        .saveOutputTo ([&bufferB, &wasCalled] (hart::AudioBuffer<float>&& outputBuffer) { wasCalled = true; bufferB = std::move (outputBuffer); })
        .process();

    HART_EXPECT_TRUE (wasCalled == true);
    HART_EXPECT_TRUE (hart::floatsEqual (bufferB.getLengthSeconds(), 1_ms, 5_us));
}

HART_TEST ("Re-using the input signal")
{
    const auto gainEnvelope = SegmentedEnvelope (decibelsToRatio (-10_dB))
        .hold (10_ms)
        .rampTo (decibelsToRatio (-6_dB), 10_ms, SegmentedEnvelope::Shape::linear)
        .hold (10_ms)
        .rampTo (decibelsToRatio (-3_dB), 10_ms, SegmentedEnvelope::Shape::linear)
        .hold (10_ms)
        .rampTo (decibelsToRatio (-1_dB), 10_ms, SegmentedEnvelope::Shape::linear);

    std::unique_ptr<hart::SignalBase<float>> reusableSignal = nullptr;

    // Fresh signal instance
    // 5ms of stable -10dB signal
    processAudioWith (GainDb (0_dB))
        .withLabel ("First section")
        //.withInputSignal (SineSweep().withDuration (200_ms) >> GainLinear().withEnvelope (GainLinear::gainLinear, gainEnvelope))
        .withInputSignal (SineWave() >> GainLinear().withEnvelope (GainLinear::gainLinear, gainEnvelope))
        .withDuration (5_ms)
        .saveInputSignalTo (reusableSignal)  // Via smart pointer reference
        .expectTrue (PeaksAt (-10_dB))
        .process();

    // Re-using it, but from whatever state it was in after "First section" test
    // -10dB for 5ms, then 10ms ramp, then 5ms of -6dB signal
    processAudioWith (GainDb (0_dB))
        .withLabel ("Second section")
        .withInputSignal (std::move (reusableSignal))  // Re-use it
        .withSignalPreparation (hart::Preparation::none)  // Keep the state of re-used signal
        .withDuration (20_ms)
        .saveInputSignalTo (reusableSignal)  // Save it via same smart pointer reference
        .expectTrue (PeaksAt (-6_dB))
        .process();

    // Now receiving it via sink function, instead of smart pointer
    auto inputSignalSink = [&reusableSignal] (std::unique_ptr<hart::SignalBase<float>>&& usedSignal)
    {
        reusableSignal = std::move (usedSignal);
    };

    // -6dB for 5ms, then 10ms ramp, then -3dB for 5ms
    processAudioWith (GainDb (0_dB))
        .withLabel ("Third section")
        .withInputSignal (std::move (reusableSignal))  // Re-use it again
        .withSignalPreparation (hart::Preparation::none)  // Keep the state of re-used signal again
        .withDuration (20_ms)
        .saveInputSignalTo (inputSignalSink)  // Save it via provided function
        .expectTrue (PeaksAt (-3_dB))
        .process();

    // Re-use it again, see if it still holds its state
    // -3dB for 5ms, then 10ms ramp, then -1dB for 5ms
    processAudioWith (GainDb (0_dB))
        .withLabel ("Fourth section")
        .withInputSignal (std::move (reusableSignal))  // And re-use it again
        .withSignalPreparation (hart::Preparation::none)  // Still keep the state of re-used signal
        .withDuration (20_ms)
        .saveInputSignalTo (reusableSignal)  // Save it via provided function once again
        .expectTrue (PeaksAt (-1_dB))
        .process();

    // Reset it back to initial state
    processAudioWith (GainDb (0_dB))
        .withLabel ("Back to first section")
        .withInputSignal (std::move (reusableSignal))  // And re-use it one last time...
        .withSignalPreparation (hart::Preparation::reset)  // ...but reset before rendering
        .withDuration (5_ms)
        .expectTrue (PeaksAt (-10_dB))
        .process();
}

HART_TEST ("Accessing DSP elements in Signal's DSP chain")
{
    std::unique_ptr<hart::SignalBase<float>> signalWithReusableDSP = nullptr;
    
    // Create some envelope to make sure the state is preserved between the two renders
    const auto gainEnvelope = SegmentedEnvelope (-3_dB)
        .hold (10_ms)
        .rampTo (-6_dB, 10_ms);

    // First render - will forward the 2nd (index 1) DSP's state to the end of the automation curve
    processAudioWith (GainDb (0_dB))
        .withInputSignal (SineWave() >> GainDb (+1_dB) >> GainDb().withEnvelope (GainDb::gainDb, gainEnvelope) >> GainDb (-1_dB))
        .withDuration (50_ms)
        .saveInputSignalTo (signalWithReusableDSP)
        .expectTrue (PeaksAt (-3_dB))
        .process();

    // Sanity check
    HART_ASSERT_TRUE (signalWithReusableDSP->getDSPChainSize() == 3);

    // Peek at the DSP with a specific index, it stays in the DSP chain
    HART_ASSERT_TRUE (signalWithReusableDSP->getDSP (0) != nullptr);
    HART_EXPECT_TRUE (floatsEqual (+1_dB, signalWithReusableDSP->getDSP (0)->getValue (GainDb::gainDb)));
    HART_ASSERT_TRUE (signalWithReusableDSP->getDSPChainSize() == 3);  // The DSP chain is still intact

    // You can do this to update the state of the DSP as well
    // Indices can be positive (counting fron the start) and negative (counting from the end)
    HART_ASSERT_TRUE (signalWithReusableDSP->getDSP (-1) != nullptr);
    HART_ASSERT_TRUE (signalWithReusableDSP->getDSP (-1) == signalWithReusableDSP->getDSP (2));  // Counting from the end vs counting from the start
    HART_EXPECT_TRUE (floatsEqual (-1_dB, signalWithReusableDSP->getDSP (-1)->getValue (GainDb::gainDb)));
    signalWithReusableDSP->getDSP (-1)->setValue (GainDb::gainDb, -15_dB);
    HART_EXPECT_TRUE (floatsEqual (-15_dB, signalWithReusableDSP->getDSP (-1)->getValue (GainDb::gainDb)));

    // Transfer a DSP instance, by removing it from the DSP chain
    std::unique_ptr<hart::DSPBase<float>> reusableDSP = signalWithReusableDSP->popDSP (-2);
    HART_ASSERT_TRUE (signalWithReusableDSP->getDSPChainSize() == 2);  // DSP element should be removed by popDSP()

    processAudioWith (std::move (reusableDSP))
        .withInputSignal (SineWave())
        .withDuration (50_ms)
        .saveInputSignalTo (signalWithReusableDSP)
        .expectTrue (PeaksAt (-6_dB))
        .process();
}

HART_TEST ("Runner - No input signal defaults to Silence")
{
    processAudioWith (GainDb (0_dB))
        .expectTrue (EqualsTo (Silence()))
        .process();
}
