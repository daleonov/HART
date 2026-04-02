#include "hart.hpp"

HART_DECLARE_ALIASES_FOR_FLOAT;

HART_TEST ("DSPSequence - Rendering")
{
    processAudioWith (HART_DSP_SEQUENCE (GainDb (-2_dB) >> GainDb (-4_dB)))
        .withLabel ("Gain accumulation - Short chain")
        .withInputSignal (SineWave())
        .expectTrue (PeaksAt (-6_dB))
        .expectTrue (EqualsTo (SineWave() >> GainDb (-6_dB)))
        .process();

    auto longGainSequence = HART_DSP_SEQUENCE (
        GainDb (-2_dB) >> GainDb (-4_dB) >> GainDb (-3_dB) >> GainDb (-1_dB) >> GainDb (-5_dB)
        );

    auto longGainSequenceReuse = processAudioWith (std::move (longGainSequence))
        .withLabel ("Gain accumulation - Longer chain")
        .withInputSignal (SineWave())
        .expectTrue (PeaksAt (-15_dB))
        .expectTrue (EqualsTo (SineWave() >> GainDb (-15_dB)))
        .process();

    auto* longGainSequenceReuseDerived = dynamic_cast<hart::DSPSequence<float>*> (longGainSequenceReuse.get());
    HART_ASSERT_TRUE (longGainSequenceReuseDerived != nullptr);

    for (int i = 0; i < 100; ++i)
    {
        const double gainSign = static_cast<double> (i % 2 == 0) * 2.0 - 1.0;
        longGainSequenceReuseDerived->append (GainDb (gainSign * 3_dB));
    }

    processAudioWith (std::move (longGainSequenceReuse))
        .withLabel ("Gain accumulation - Very long chain")
        .withInputSignal (SineWave())
        .expectTrue (PeaksAt (-15_dB))
        .expectTrue (EqualsTo (SineWave() >> GainDb (-15_dB)))
        .process();

    auto nestedDspSequence = HART_DSP_SEQUENCE (GainDb (-2_dB) >> GainDb (-4_dB));
    processAudioWith (HART_DSP_SEQUENCE (GainDb (-7_dB) >> nestedDspSequence.move() >> GainDb (-3_dB)))
        .withLabel ("Gain accumulation - Nested sequence")
        .withInputSignal (SineWave())
        .expectTrue (PeaksAt (-16_dB))
        .expectTrue (EqualsTo (SineWave() >> GainDb (-16_dB)))
        .process();

    processAudioWith (DSPSequence ({}))
        .withLabel ("Empty chain - No audio output")
        .withInputSignal (SineWave())
        .expectTrue (EqualsTo (Silence()))
        .process();
}

HART_TEST ("DSPSequence - Container properties")
{
    auto dspSequence = HART_DSP_SEQUENCE (GainDb (-2_dB));
    HART_EXPECT_TRUE (dspSequence.size() == 1);

    dspSequence.append (HardClip (0_dB));
    dspSequence.append (hart::make_unique<HardClip> (0_dB));

    HART_EXPECT_TRUE (dspSequence.size() == 3);

    const auto copyMe = HardClip (-6_dB);
    dspSequence.append (copyMe.copy());

    HART_EXPECT_TRUE (dspSequence.size() == 4);

    auto moveMeAsRvalueRef = HardClip (-3_dB);
    auto moveMeAsUniquePtr = HardClip (-4_dB);
    dspSequence.append (std::move (moveMeAsRvalueRef));
    dspSequence.append (moveMeAsUniquePtr.move());

    HART_EXPECT_TRUE (dspSequence.size() == 6);

    const auto* peekMeA = dspSequence[0];
    auto* peekMeB = dspSequence[0];
    const auto* peekMeC = dspSequence[-1];
    auto* peekMeD = dspSequence[-1];
    auto* peekMeE = dspSequence[5];  // Same as -1 for size = 6

    HART_EXPECT_TRUE (peekMeA != nullptr);
    HART_EXPECT_TRUE (peekMeB != nullptr);
    HART_EXPECT_TRUE (peekMeC != nullptr);
    HART_EXPECT_TRUE (peekMeD != nullptr);
    HART_EXPECT_TRUE (peekMeE != nullptr);
    HART_EXPECT_TRUE (peekMeE == peekMeD);  // Can count same element from beginning and from the end
    HART_EXPECT_TRUE (dspSequence.size() == 6);

    using hart::floatsEqual;

    std::unique_ptr<hart::DSPBase<float>> lastDspInChainButNotAnymore = dspSequence.pop();
    HART_EXPECT_TRUE (dspSequence.size() == 5);
    HART_EXPECT_TRUE (lastDspInChainButNotAnymore != nullptr);
    HART_EXPECT_TRUE (dynamic_cast<HardClip*> (lastDspInChainButNotAnymore.get()) != nullptr);
    HART_EXPECT_TRUE (floatsEqual (-4_dB, lastDspInChainButNotAnymore->getValue (HardClip::thresholdDb)));

    std::unique_ptr<hart::DSPBase<float>> firstDspInChainButNotAnymore = dspSequence.pop (0);  // Non-vegative index
    HART_EXPECT_TRUE (dspSequence.size() == 4);
    HART_EXPECT_TRUE (firstDspInChainButNotAnymore != nullptr);
    HART_EXPECT_TRUE (dynamic_cast<GainDb*> (firstDspInChainButNotAnymore.get()) != nullptr);
    HART_EXPECT_TRUE (floatsEqual (-2_dB, firstDspInChainButNotAnymore->getValue (GainDb::gainDb)));

    std::unique_ptr<hart::DSPBase<float>> dspSomewhereInTheMiddleOfTheChainButNotAnymore = dspSequence.pop (-2);  // Negative index
    HART_EXPECT_TRUE (dspSequence.size() == 3);
    HART_EXPECT_TRUE (dspSomewhereInTheMiddleOfTheChainButNotAnymore != nullptr);
    HART_EXPECT_TRUE (dynamic_cast<HardClip*> (dspSomewhereInTheMiddleOfTheChainButNotAnymore.get()) != nullptr);
    HART_EXPECT_TRUE (floatsEqual (-6_dB, dspSomewhereInTheMiddleOfTheChainButNotAnymore->getValue (HardClip::thresholdDb)));
}

HART_TEST ("DSPSequence - Deep copy")
{
    auto nestedDspSequence = HART_DSP_SEQUENCE (GainDb (-2_dB) >> GainDb (-4_dB));

    const DSPSequence originalDspSequence = HART_DSP_SEQUENCE (HardClip (-8_dB) >> nestedDspSequence.move() >> HardClip (-5_dB));
    const std::unique_ptr<hart::DSPBase<float>> clonedDspSequence = originalDspSequence.copy();
    const DSPSequence* clonedDspSequenceDerivedPtr = dynamic_cast<const DSPSequence*> (clonedDspSequence.get());
    HART_ASSERT_TRUE (clonedDspSequenceDerivedPtr != nullptr);

    const DSPSequence* dspSequenceNestedInOriginal = dynamic_cast<const DSPSequence*> (originalDspSequence[1]);
    const DSPSequence* dspSequenceNestedInClone = dynamic_cast<const DSPSequence*> ((*clonedDspSequenceDerivedPtr)[1]);

    HART_ASSERT_TRUE (dspSequenceNestedInOriginal != nullptr);
    HART_ASSERT_TRUE (dspSequenceNestedInClone != nullptr);

    HART_ASSERT_TRUE (dspSequenceNestedInOriginal->size() == 2);
    HART_ASSERT_TRUE (dspSequenceNestedInClone->size() == dspSequenceNestedInOriginal->size());

    const GainDb* originalNestedDsp = dynamic_cast<const GainDb*> ((*dspSequenceNestedInOriginal)[0]);
    const GainDb* clonedNestedDsp = dynamic_cast<const GainDb*> ((*dspSequenceNestedInClone)[0]);

    HART_ASSERT_TRUE (originalNestedDsp != nullptr);
    HART_ASSERT_TRUE (clonedNestedDsp != nullptr);

    HART_EXPECT_TRUE (originalNestedDsp != clonedNestedDsp);  // Most important thing! Expecting independent objects

    using hart::floatsEqual;
    HART_EXPECT_TRUE (floatsEqual (-2_dB, originalNestedDsp->getValue (GainDb::gainDb)));
    HART_EXPECT_TRUE (floatsEqual (-2_dB, clonedNestedDsp->getValue (GainDb::gainDb)));
}
