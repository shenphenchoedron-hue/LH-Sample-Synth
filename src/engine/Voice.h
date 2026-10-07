#pragma once

#include <cstdint>
#include <vector>

#include "EngineParams.h"
#include "Envelope.h"
#include "SampleData.h"
#include "../dsp/FormantProcessor.h"
#include "../dsp/GranularPitchProcessor.h"
#include "../dsp/NaturalPlaybackProcessor.h"

namespace lhss
{
/** Per-block values shared (read-only) by all voices. */
struct VoiceContext
{
    const EngineParams* params = nullptr;
    double hostRate = 44100.0;
    double pitchBendRatio = 1.0;
    float formant = 0.0f;      // smoothed
    float grainSizeMs = 80.0f; // smoothed
};

/** One polyphonic voice.

    Ownership / realtime rules: a Voice is owned by VoiceManager and only touched by the audio
    thread. Every piece of mutable state (note, velocity, playhead, envelope, grain pool,
    loop/reverse/release flags, formant filter state, scratch buffers) is a member of this
    object — nothing mutable is shared between voices. Scratch buffers are allocated in
    prepare() (never during playback).

    The voice holds a raw pointer to the SampleData it started with and keeps it registered
    in SampleData::voiceRefs until it stops, so a newly loaded sample never pulls memory out
    from under a sounding note (see SampleStore). */
class Voice
{
public:
    void prepare (double hostRate, int maxBlockSize, std::uint32_t seed);

    void start (const SampleData* sample, int midiNote, float velocity01, std::uint64_t age, const EngineParams& params) noexcept;
    void noteOff (const EngineParams& params) noexcept;
    void steal() noexcept;      // short click-free fade, then the voice frees itself
    void forceStop() noexcept;  // immediate stop (no fade)

    /** Renders and adds `n` samples into the mix. n <= maxBlockSize. */
    void render (const VoiceContext& ctx, float* mixL, float* mixR, int n) noexcept;

    bool isActive() const noexcept { return active; }
    bool isKilling() const noexcept { return active && envelope.getStage() == Envelope::Stage::Kill; }
    bool isReleased() const noexcept { return released || envelope.isReleasing(); }
    bool hasReceivedNoteOff() const noexcept { return released; }
    int getNote() const noexcept { return note; }
    std::uint64_t getAge() const noexcept { return age; }
    const SampleData* getSample() const noexcept { return sample; }
    const dsp::PlayheadState& getPlayhead() const noexcept { return playhead; }
    const Envelope& getEnvelope() const noexcept { return envelope; }

private:
    enum class RenderMode { Natural, Granular };

    void stopAndRelease() noexcept;
    static RenderMode wantedMode (const EngineParams& p) noexcept;

    double hostRate = 44100.0;
    std::uint32_t seed = 1;
    std::vector<float> bufL, bufR;

    bool active = false, released = false;
    int note = 60;
    float velocityGain = 1.0f;
    std::uint64_t age = 0;
    const SampleData* sample = nullptr;

    Envelope envelope;
    dsp::PlayheadState playhead, tailPlayhead;
    double tailIncrement = 1.0;
    RenderMode mode = RenderMode::Natural;
    dsp::GranularPitchProcessor granular;
    dsp::NaturalPlaybackProcessor natural, naturalTail;
    dsp::FormantProcessor formant;
};
} // namespace lhss
