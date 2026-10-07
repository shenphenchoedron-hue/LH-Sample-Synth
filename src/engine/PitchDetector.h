#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

namespace lhss
{
/** Result of analysing a recording's fundamental pitch. */
struct PitchInfo
{
    bool valid = false;         // false: no clear, stable pitch (noise, clicks, inharmonic sounds)
    double midiNote = 0.0;      // fractional MIDI note (69.0 = A 440 Hz), only meaningful when valid
    double frequencyHz = 0.0;
    float confidence = 0.0f;    // 0..1

    /** Nearest whole MIDI note (the Root Note). */
    int nearestNote() const noexcept { return juce::jlimit (0, 127, juce::roundToInt (midiNote)); }
    /** Offset from the nearest note in cents (-50 .. +50); the sample sounds this much sharp. */
    float centsOffset() const noexcept { return static_cast<float> ((midiNote - nearestNote()) * 100.0); }
};

/** Estimates the fundamental pitch of a recording with the YIN algorithm (de Cheveigné & Kawahara,
    2002), evaluated on many frames across the loud part of the sound and combined with a weighted
    median, so attacks, noise and the odd octave error do not decide the result. The estimate is then
    sharpened with a long FFT: the strongest spectral peak near it is what a tuner and the ear hear
    (YIN alone reads beating or inharmonic sounds such as glass up to ~30 cents off).

    Runs on the loader thread (allocates, ~tens of ms for typical samples) — never on the audio thread. */
class PitchDetector
{
public:
    static constexpr double kMinHz = 40.0;
    static constexpr double kMaxHz = 4200.0;   // top of the 88-key range (C8 = 4186 Hz)

    static PitchInfo analyse (const juce::AudioBuffer<float>& audio, double sampleRate);
};
} // namespace lhss
