#include <memory>
#include "hart.hpp"

using hart::processAudioWith;
using EqualsTo = hart::EqualsTo<float>;
using GainDb = hart::GainDb<float>;
using SineWave = hart::SineWave<float>;

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
    auto ownMe = std::make_unique<GainDb>();
    processAudioWith (std::move (ownMe))
        .withInputSignal (SineWave())
        .expectTrue (EqualsTo (SineWave()))
        .process();

    // ...or like so:
    processAudioWith (std::make_unique<GainDb>())
        .withInputSignal (SineWave())
        .expectTrue (EqualsTo (SineWave()))
        .process();
}
