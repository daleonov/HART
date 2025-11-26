#include <memory>
#include "hart.hpp"

HART_DECLARE_ALIASES_FOR_FLOAT;

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
    const GainDb copyMe;
    processAudioWith (copyMe)
        .withInputSignal (SineWave())
        .expectTrue (EqualsTo (SineWave()))
        .process();

    // ...and of course it should be reusable after copying:
    processAudioWith (copyMe)
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
    const GainDb copyMe;
    std::unique_ptr<hart::DSPBase<float>> ownMeAsAbstract = hart::make_unique<GainDb>();
    std::unique_ptr<GainDb> ownMeAsDerived = hart::make_unique<GainDb>();

    processAudioWith (GainDb (0_dB))
        .withInputSignal (SineWave() >> moveMe >> copyMe >> std::move (ownMeAsAbstract) >> std::move (ownMeAsDerived))
        .process();
}
