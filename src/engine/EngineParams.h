#pragma once

#include <cmath>

namespace lhss
{
enum class PlaybackMode { Natural = 0, Pitch = 1 };
enum class TriggerMode  { OneShot = 0, Gate = 1 };
enum class FilterMode   { LowPass = 0, HighPass = 1, BandPass = 2 };
enum class LfoShape     { Sine = 0, Triangle = 1, Square = 2, Saw = 3, Random = 4 };
enum class VoiceMode    { Poly = 0, Mono = 1, Legato = 2 };

inline constexpr int kMaxPolyphony = 16;
inline constexpr int kMaxUnison = 4;

/** Tempo-sync divisions (names shown in the GUI) and their length in quarter-note beats. */
inline constexpr const char* kSyncDivisionNames[] = { "1/1", "1/2", "1/4", "1/8", "1/16", "1/32",
                                                      "1/2T", "1/4T", "1/8T", "1/16T",
                                                      "1/2.", "1/4.", "1/8.", "1/16." };
inline constexpr double kSyncDivisionBeats[] = { 4.0, 2.0, 1.0, 0.5, 0.25, 0.125,
                                                 4.0 / 3.0, 2.0 / 3.0, 1.0 / 3.0, 1.0 / 6.0,
                                                 3.0, 1.5, 0.75, 0.375 };
inline constexpr int kNumSyncDivisions = 14;

inline double syncDivisionSeconds (int index, double bpm) noexcept
{
    const int i = index < 0 ? 0 : (index >= kNumSyncDivisions ? kNumSyncDivisions - 1 : index);
    return kSyncDivisionBeats[i] * 60.0 / (bpm > 1.0 ? bpm : 120.0);
}

/** Plain snapshot of every engine parameter, filled by the plugin wrapper once per block.
    The engine never touches APVTS or any plugin-format API. */
struct EngineParams
{
    int rootNote = 60;
    PlaybackMode playbackMode = PlaybackMode::Pitch;
    float sampleStart = 0.0f, sampleEnd = 1.0f;        // normalised to file length
    float attackMs = 5.0f, decayMs = 250.0f, sustain = 0.5f, releaseMs = 400.0f;
    TriggerMode triggerMode = TriggerMode::OneShot;
    bool loopOn = false;
    float loopStart = 0.25f, loopEnd = 0.75f;          // normalised to file length
    float loopCrossfadeMs = 50.0f;
    float grainSizeMs = 80.0f, grainDensity = 8.0f;
    float grainPosRandom = 0.05f, pitchRandom = 0.0f, stereoSpread = 0.3f;
    bool freeze = false, reverse = false;
    FilterMode filterMode = FilterMode::LowPass;
    float cutoffHz = 20000.0f, resonance = 0.1f;
    float formant = 0.0f;                              // -1 .. +1
    float pan = 0.0f, stereoWidth = 1.0f, outputGainDb = 0.0f;
    float velocitySensitivity = 1.0f;
    int polyphony = kMaxPolyphony;

    // LFO (one global LFO, free running or tempo synced)
    LfoShape lfoShape = LfoShape::Sine;
    float lfoRateHz = 4.0f;
    bool lfoSync = false;
    int lfoDivision = 2;
    float lfoToPitch = 0.0f;      // semitones
    float lfoToCutoff = 0.0f;     // -1..1 → ±4 octaves
    float lfoToAmp = 0.0f;        // 0..1 tremolo depth
    float lfoToPan = 0.0f;        // 0..1
    float lfoToGrainPos = 0.0f;   // 0..1 → ±250 ms grain start offset

    // Filter envelope
    float fenvAttackMs = 1.0f, fenvDecayMs = 300.0f, fenvSustain = 0.0f, fenvReleaseMs = 300.0f;
    float fenvAmount = 0.0f;      // -1..1 → ±6 octaves
    float velToFilter = 0.0f;     // 0..1 → soft notes up to 4 octaves darker

    // Voice
    VoiceMode voiceMode = VoiceMode::Poly;
    float glideMs = 0.0f;
    int coarseTune = 0;           // semitones
    float fineTune = 0.0f;        // cents
    int unisonVoices = 1;
    float unisonDetune = 12.0f;   // cents (outer voices)
    float drive = 0.0f;           // 0..1

    // Effects
    float chorusRate = 0.8f, chorusDepth = 0.5f, chorusMix = 0.0f;
    float delayTimeMs = 375.0f;
    bool delaySync = false;
    int delayDivision = 12;
    float delayFeedback = 0.35f, delayMix = 0.0f;
    bool delayPingPong = false;
    float reverbSize = 0.6f, reverbDamping = 0.5f, reverbMix = 0.0f;
};

/** pitchRatio = 2^((playedNote - rootNote) / 12). */
inline double pitchRatio (int playedNote, int rootNote) noexcept
{
    return std::pow (2.0, (playedNote - rootNote) / 12.0);
}

/** Source frames to advance per host output sample so that the file plays at its
    original speed regardless of the host rate (sample-rate conversion factor). */
inline double sourceIncrement (double sourceRate, double hostRate) noexcept
{
    return hostRate > 0.0 ? sourceRate / hostRate : 1.0;
}

/** Velocity → amplitude. A power curve (≈ -10 dB at half velocity with full sensitivity),
    blended towards a flat response by `sensitivity`. */
inline float velocityToGain (float velocity01, float sensitivity) noexcept
{
    const float v = velocity01 < 0.0f ? 0.0f : (velocity01 > 1.0f ? 1.0f : velocity01);
    return (1.0f - sensitivity) + sensitivity * std::pow (v, 1.6f);
}
} // namespace lhss
