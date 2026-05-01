#include "hart.hpp"

HART_DECLARE_ALIASES_FOR_FLOAT;
using hart::decibelsToRatio;

HART_TEST ("Envelope - Gain Envelope Regression")
{
    HART_REQUIRES_DATA_PATH_ARG;

    const auto gainEnvelopeA = SegmentedEnvelope (decibelsToRatio (-10_dB))
        .hold (5_ms)
        .rampTo (decibelsToRatio (0_dB), 25_ms, SegmentedEnvelope::Shape::sCurve)
        .hold (5_ms)
        .rampTo (decibelsToRatio (-10_dB), 35_ms, SegmentedEnvelope::Shape::sCurve);

    processAudioWith (GainLinear().withEnvelope (GainLinear::gainLinear, gainEnvelopeA))
        .withLabel ("Envelope A")
        .inMono()
        .withInputSignal (SineWave (2_kHz))
        .withSampleRate (44100_Hz)
        .withDuration (75_ms)
        .saveOutputTo ("Gain Envelope A Fail.wav", hart::Save::whenFails)
        .expectTrue (EqualsTo (WavFile ("Gain Envelope A.wav")))
        .process();

    const auto gainEnvelopeB = SegmentedEnvelope (decibelsToRatio (-12_dB))
        .hold (5_ms)
        .rampTo (decibelsToRatio (-1_dB), 30_ms, SegmentedEnvelope::Shape::linear)
        .hold (5_ms)
        .rampTo (decibelsToRatio (-9_dB), 30_ms, SegmentedEnvelope::Shape::linear);

    processAudioWith (GainLinear().withEnvelope (GainLinear::gainLinear, gainEnvelopeB))
        .withLabel ("Envelope B")
        .inMono()
        .withInputSignal (SineWave (3_kHz))
        .withSampleRate (44100_Hz)
        .withDuration (75_ms)
        .saveOutputTo ("Gain Envelope B Fail.wav", hart::Save::whenFails)
        .expectTrue (EqualsTo (WavFile ("Gain Envelope B.wav")))
        .process();

    const auto gainEnvelopeC = SegmentedEnvelope (decibelsToRatio (-1_dB))
        .hold (5_ms)
        .rampTo (decibelsToRatio (-10_dB), 28_ms, SegmentedEnvelope::Shape::exponential)
        .hold (5_ms)
        .rampTo (decibelsToRatio (-3_dB), 32_ms, SegmentedEnvelope::Shape::exponential);

    processAudioWith (GainLinear().withEnvelope (GainLinear::gainLinear, gainEnvelopeC))
        .withLabel ("Envelope C")
        .inMono()
        .withInputSignal (SineWave (2.5_kHz))
        .withSampleRate (44100_Hz)
        .withDuration (75_ms)
        .saveOutputTo ("Gain Envelope C Fail.wav", hart::Save::whenFails)
        .expectTrue (EqualsTo (WavFile ("Gain Envelope C.wav")))
        .process();
}
