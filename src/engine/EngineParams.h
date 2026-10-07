#pragma once

#include <cmath>

namespace lhss
{
enum class PlaybackMode { Natural = 0, Pitch = 1 };
enum class TriggerMode  { OneShot = 0, Gate = 1 };
enum class FilterMode   { LowPass = 0, HighPass = 1, BandPass = 2 };

inline constexpr int kMaxPolyphony = 16;

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
