#pragma once

#include <array>
#include <cstdint>

#include "Grain.h"
#include "SampleReader.h"

namespace lhss::dsp
{
/** Duration-preserving granular pitch shifter (one instance per voice).

    How it works
    ------------
    Two clocks run independently:
      * The *traversal* clock moves the voice playhead through the source at the source's
        natural speed (sourceRate / hostRate frames per output sample). This sets the
        overall duration: a 1.2 s file still takes ≈1.2 s at every MIDI note.
      * Every `grainSize / density` output samples a new grain is spawned at the playhead.
        Each grain reads the source at `naturalSpeed * pitchRatio`, i.e. its *content* is
        transposed, and is shaped by a Hann window. Overlapping windows (density ≥ 2) sum
        to an approximately constant gain, which is normalised by 2/density.
    Because pitch lives inside the grains and duration lives in the traversal clock, pitch and
    time are decoupled. A grain's start point is offset by L/2·(travel − rate) so that the
    grain's centre lines up with the playhead at the grain's centre time — this keeps
    transients in place for both upward and downward shifts.

    Phase alignment (WSOLA-style): with plain fixed-interval grains, consecutive overlapping
    grains read tonal material with a phase offset of interval·(travel − rate) samples, which
    causes comb cancellation and an audible frequency error. Before a grain starts, its start
    point is therefore moved within ±tolerance (≤ 12 ms / ¼ grain) to the position whose
    waveform best matches (normalised cross-correlation) what the most recent grain is reading
    right now. The new grain then continues the old one in phase, giving clean, correctly tuned
    pitch shifting. Alignment fades out as Position/Pitch Randomness increase, where incoherent
    grains are the desired synthetic texture.

    Synthetic textures come from Position Randomness (random start offsets), Pitch Randomness
    (per-grain detune up to ±12 semitones, quadratic curve), Stereo Spread (per-grain
    constant-power pan) and low densities / large grains.

    Freeze stops the traversal clock while grains keep spawning (with a minimum position
    jitter so a frozen spot does not buzz at the grain rate). Reverse flips both the traversal
    direction and the grain read direction.

    Realtime: the grain pool and the window table are preallocated members; render() never
    allocates or locks. */
class GranularPitchProcessor
{
public:
    static constexpr int kMaxGrains = 64;
    static constexpr int kWindowSize = 1024;

    struct Settings
    {
        double hostRate = 44100.0;
        double baseIncrement = 1.0;     // sourceRate / hostRate
        double pitchRatio = 1.0;
        double travelSpeed = 1.0;       // multiple of baseIncrement for the playhead (0 = frozen)
        float grainSizeMs = 80.0f;
        float density = 8.0f;           // average overlapping grains
        float positionRandom = 0.0f;    // 0..1
        float pitchRandom = 0.0f;       // 0..1
        float stereoSpread = 0.0f;      // 0..1
        double positionOffset = 0.0;    // source frames added to every new grain's start (LFO)
    };

    GranularPitchProcessor();

    void reset (std::uint32_t seed) noexcept;

    /** Renders n samples, adding into outL/outR.
        @param ownsPlayhead     this processor owns the voice playhead (moves it)
        @param spawn            new grains may be created */
    void render (const SourceView& src, const ResolvedRegion& region, PlayheadState& playhead,
                 bool ownsPlayhead, bool spawn, const Settings& settings,
                 float* outL, float* outR, int n) noexcept;

    int getActiveGrainCount() const noexcept { return activeGrains; }

private:
    void spawnGrain (const SourceView& src, const ResolvedRegion& region, const PlayheadState& ph,
                     const Settings& s, int lengthSamples) noexcept;
    double alignStart (const SourceView& src, double nominal, const Grain& reference, double toleranceFrames) const noexcept;
    float random01() noexcept;      // [0, 1)
    float randomBipolar() noexcept; // [-1, 1)

    std::array<Grain, kMaxGrains> grains {};
    std::array<float, kWindowSize + 1> window {};
    int activeGrains = 0;
    int lastSpawned = -1;
    double spawnCountdown = 0.0;
    std::uint32_t rng = 0x12345678u;
};
} // namespace lhss::dsp
