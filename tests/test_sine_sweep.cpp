#include "hart.hpp"

using hart::decibelsToRatio;
using hart::processAudioWith;
using EqualsTo = hart::EqualsTo<float>;
using PeaksAt = hart::PeaksAt<float>;
using PeaksBelow = hart::PeaksBelow<float>;
using GainDb = hart::GainDb<float>;
using SegmentedEnvelope = hart::SegmentedEnvelope;
using Silence = hart::Silence<float>;
using SineSweep = hart::SineSweep<float>;
using SineWave = hart::SineWave<float>;
using WavFile = hart::WavFile<float>;

HART_TEST ("SineSweep - Normal Use")
{
    processAudioWith (GainDb (0_dB))
        .withDuration (2.1_s)
        .withInputSignal (SineSweep (2_s) >> GainDb (-1_dB))
        .expectTrue (PeaksAt (-1_dB))
        .process();

    processAudioWith (GainDb (0_dB))
        .withDuration (2.1_s)
        .withInputSignal (SineSweep (2_s).withType (SineSweep::SweepType::linear) >> GainDb (-1_dB))
        .expectTrue (PeaksAt (-1_dB))
        .process();

    processAudioWith (GainDb (0_dB))
        .withSampleRate (96_kHz)
        .withDuration (2.1_s)
        .withInputSignal (SineSweep (2_s) >> GainDb (-1_dB))
        .expectTrue (PeaksAt (-1_dB))
        .process();
}

HART_TEST ("SineSweep - Lin vs Log")
{
    processAudioWith (GainDb (0_dB))
        .withDuration (200_ms)
        .withInputSignal (SineSweep (200_ms).withType (SineSweep::SweepType::linear))
        .expectFalse (EqualsTo (SineSweep (200_ms).withType (SineSweep::SweepType::log)))
        .process();
}

HART_TEST ("SineSweep - Zero Duration")
{
    processAudioWith (GainDb (0_dB))
        .withDuration (100_ms)
        .withInputSignal (SineSweep (0_s))
        .expectTrue (PeaksBelow (-60_dB))
        .expectTrue (EqualsTo (Silence()))
        .process();
}

HART_TEST ("SineSweep - Loop")
{
    processAudioWith (GainDb (0_dB))
        .withLabel ("Actually loops if requested")
        .withDuration (350_ms)
        .withInputSignal (SineSweep (300_ms).withLoop (SineSweep::Loop::yes))
        .expectTrue (PeaksAt (0_dB))
        .expectFalse (EqualsTo (SineSweep (300_ms).withLoop (SineSweep::Loop::no)))
        .process();
}

HART_TEST ("SineSweep - Fixed Frequency")
{
    processAudioWith (GainDb (0_dB))
        .withDuration (350_ms)
        .withInputSignal (SineSweep (300_ms, 1234_Hz, 1234_Hz, SineSweep::SweepType::log, SineSweep::Loop::yes))
        .expectTrue (EqualsTo (SineWave (1234_Hz)))
        .process();

    processAudioWith (GainDb (0_dB))
        .withDuration (350_ms)
        .withInputSignal (SineSweep (300_ms, 1234_Hz, 1234_Hz, SineSweep::SweepType::linear, SineSweep::Loop::yes))
        .expectTrue (EqualsTo (SineWave (1234_Hz)))
        .process();
}

HART_TEST ("SineSweep - Initial Phase")
{
    processAudioWith (GainDb (0_dB))
        .withDuration (300_ms)
        .withInputSignal (SineSweep (300_ms))
        .expectTrue (PeaksAt (0_dB))
        .expectFalse (EqualsTo (SineSweep (300_ms).withPhase (hart::halfPi)))
        .expectFalse (EqualsTo (SineSweep (300_ms).withPhase (hart::pi)))
        .expectTrue (EqualsTo (SineSweep (300_ms).withPhase (hart::twoPi)))
        .expectTrue (EqualsTo (SineSweep (300_ms).withPhase (8 * hart::twoPi)))
        .process();
}
