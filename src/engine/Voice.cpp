#include "Voice.h"
#include "SampleRegion.h"

#include <algorithm>
#include <cstring>

namespace lhss
{
namespace
{
constexpr float kVoiceHeadroom = 0.5f; // -6 dB per voice; the output stage handles summing
constexpr double kModeFadeSeconds = 0.006;

dsp::ResolvedRegion regionFor (const SampleData& s, const EngineParams& p) noexcept
{
    return resolveRegion (s.getNumFrames(), s.getSampleRate(), p.sampleStart, p.sampleEnd,
                          p.loopOn, p.loopStart, p.loopEnd, p.loopCrossfadeMs);
}
} // namespace

void Voice::prepare (double rate, int maxBlockSize, std::uint32_t voiceSeed)
{
    forceStop();
    hostRate = rate;
    seed = voiceSeed;
    bufL.assign ((size_t) std::max (1, maxBlockSize), 0.0f);
    bufR.assign ((size_t) std::max (1, maxBlockSize), 0.0f);
    envelope.setSampleRate (rate);
}

Voice::RenderMode Voice::wantedMode (const EngineParams& p) noexcept
{
    // Freeze needs grains even in Natural mode (a stopped playhead cannot be resampled).
    return (p.playbackMode == PlaybackMode::Pitch || p.freeze) ? RenderMode::Granular : RenderMode::Natural;
}

void Voice::start (const SampleData* s, int midiNote, float velocity01, std::uint64_t voiceAge, const EngineParams& p) noexcept
{
    if (active) stopAndRelease();
    if (s == nullptr || s->getNumFrames() < 2) return;

    sample = s;
    sample->voiceRefs.fetch_add (1, std::memory_order_acq_rel);

    active = true;
    released = false;
    note = midiNote;
    age = voiceAge;
    velocityGain = velocityToGain (velocity01, p.velocitySensitivity);

    const auto region = regionFor (*s, p);
    playhead = {};
    playhead.direction = p.reverse ? -1 : 1;
    playhead.position = p.reverse ? region.end - 1.0 : region.start;
    playhead.looping = region.loopOn;

    envelope.reset();
    envelope.setParameters (p.attackMs, p.decayMs, p.sustain, p.releaseMs);
    envelope.noteOn();

    mode = wantedMode (p);
    granular.reset (seed * 2654435761u + static_cast<std::uint32_t> (voiceAge));
    natural.reset();
    naturalTail.reset();
    naturalTail.startFadeOut (1); // silent tail
    formant.reset();
}

void Voice::noteOff (const EngineParams& p) noexcept
{
    if (! active || released) return;
    released = true;
    if (p.triggerMode == TriggerMode::Gate) envelope.noteOff();
    // One Shot: keep playing to the end of the region (the loop is exited in render()).
}

void Voice::steal() noexcept
{
    if (active) envelope.kill (4.0f);
}

void Voice::forceStop() noexcept
{
    if (active) stopAndRelease();
}

void Voice::stopAndRelease() noexcept
{
    if (sample != nullptr) sample->voiceRefs.fetch_sub (1, std::memory_order_acq_rel);
    sample = nullptr;
    active = false;
    released = false;
    envelope.reset();
}

void Voice::render (const VoiceContext& ctx, float* mixL, float* mixR, int n) noexcept
{
    if (! active || sample == nullptr || n <= 0) return;
    n = std::min (n, static_cast<int> (bufL.size()));

    const EngineParams& p = *ctx.params;
    const auto view = sample->view();
    const auto region = regionFor (*sample, p);
    const bool oneShot = p.triggerMode == TriggerMode::OneShot;

    // Region can be changed live: keep the playhead legal.
    if (! playhead.ended) playhead.position = std::clamp (playhead.position, region.start, region.end);
    playhead.direction = p.reverse ? -1 : 1;
    // In One Shot the loop holds while the key is down and lets go at note-off.
    playhead.looping = region.loopOn && ! (oneShot && released);

    envelope.setParameters (p.attackMs, p.decayMs, p.sustain, p.releaseMs);
    // A frozen one-shot would never reach the region end, so note-off releases it.
    if (released && oneShot && p.freeze && ! envelope.isReleasing()) envelope.noteOff();

    const int modeFade = static_cast<int> (kModeFadeSeconds * hostRate);
    const double baseIncrement = sourceIncrement (view.sampleRate, hostRate);
    const double ratio = pitchRatio (note, p.rootNote) * ctx.pitchBendRatio;
    const double naturalIncrement = baseIncrement * ratio;

    const auto wanted = wantedMode (p);
    if (wanted != mode)
    {
        if (mode == RenderMode::Natural)
        {
            naturalTail = natural; // crossfade the outgoing resampler out
            tailPlayhead = playhead;
            tailIncrement = naturalIncrement;
            naturalTail.startFadeOut (modeFade);
        }
        else
        {
            natural.startFadeIn (modeFade); // grains in flight finish by themselves
        }
        mode = wanted;
    }

    std::memset (bufL.data(), 0, sizeof (float) * (size_t) n);
    std::memset (bufR.data(), 0, sizeof (float) * (size_t) n);

    const bool granularMode = mode == RenderMode::Granular;
    if (granularMode || granular.getActiveGrainCount() > 0)
    {
        dsp::GranularPitchProcessor::Settings s;
        s.hostRate = hostRate;
        s.baseIncrement = baseIncrement;
        s.pitchRatio = ratio;
        s.travelSpeed = p.freeze ? 0.0 : (p.playbackMode == PlaybackMode::Pitch ? 1.0 : ratio);
        s.grainSizeMs = ctx.grainSizeMs;
        s.density = p.grainDensity;
        s.positionRandom = p.grainPosRandom;
        s.pitchRandom = p.pitchRandom;
        s.stereoSpread = p.stereoSpread;
        granular.render (view, region, playhead, granularMode, granularMode, s, bufL.data(), bufR.data(), n);
    }

    if (! granularMode) natural.render (view, region, playhead, naturalIncrement, bufL.data(), bufR.data(), n);
    if (! naturalTail.isSilent())
        naturalTail.render (view, region, tailPlayhead, tailIncrement, bufL.data(), bufR.data(), n);

    if (granularMode && p.playbackMode == PlaybackMode::Pitch)
        formant.process (bufL.data(), bufR.data(), n, ratio, ctx.formant);

    const float vg = velocityGain * kVoiceHeadroom;
    for (int i = 0; i < n; ++i)
    {
        const float g = envelope.next() * vg;
        mixL[i] += bufL[(size_t) i] * g;
        mixR[i] += bufR[(size_t) i] * g;
    }

    const bool sourceDone = playhead.ended && granular.getActiveGrainCount() == 0
                            && (naturalTail.isSilent() || tailPlayhead.ended);
    if (! envelope.isActive() || sourceDone) stopAndRelease();
}
} // namespace lhss
