#include "Voice.h"
#include "SampleRegion.h"
#include "../dsp/Effects.h"

#include <cmath>

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
    filterEnvelope.setSampleRate (rate);
    filter.prepare (rate, 0.0015); // fast smoothing: modulation arrives in ~1.3 ms chunks
}

void Voice::setGlide (double fromNote, float glideMs) noexcept
{
    const double samples = glideMs * 0.001 * hostRate;
    if (fromNote < 0.0 || samples < 1.0)
    {
        currentNote = note;
        glideStep = 0.0;
        return;
    }
    currentNote = fromNote;
    glideStep = std::abs (note - fromNote) / samples; // constant-time glide
}

void Voice::retarget (int midiNote, float glideMs) noexcept
{
    if (! active) return;
    const double from = currentNote;
    note = midiNote;
    setGlide (from, glideMs);
}

Voice::RenderMode Voice::wantedMode (const EngineParams& p) noexcept
{
    // Freeze needs grains even in Natural mode (a stopped playhead cannot be resampled).
    return (p.playbackMode == PlaybackMode::Pitch || p.freeze) ? RenderMode::Granular : RenderMode::Natural;
}

void Voice::start (const SampleData* s, int midiNote, float velocity, std::uint64_t voiceAge, const EngineParams& p,
                   const VoiceStartOptions& options) noexcept
{
    if (active) stopAndRelease();
    if (s == nullptr || s->getNumFrames() < 2) return;

    sample = s;
    sample->voiceRefs.fetch_add (1, std::memory_order_acq_rel);

    active = true;
    released = false;
    note = midiNote;
    age = voiceAge;
    velocity01 = velocity;
    velocityGain = velocityToGain (velocity, p.velocitySensitivity);
    detuneCents = options.detuneCents;
    panOffset = options.panOffset;
    setGlide (options.glideFromNote, p.glideMs);
    firstChunk = true;

    const auto region = regionFor (*s, p);
    playhead = {};
    playhead.direction = p.reverse ? -1 : 1;
    playhead.position = p.reverse ? region.end - 1.0 : region.start;
    playhead.looping = region.loopOn;

    envelope.reset();
    envelope.setParameters (p.attackMs, p.decayMs, p.sustain, p.releaseMs);
    envelope.noteOn();
    filterEnvelope.reset();
    filterEnvelope.setParameters (p.fenvAttackMs, p.fenvDecayMs, p.fenvSustain, p.fenvReleaseMs);
    filterEnvelope.noteOn();
    filter.reset();

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
    if (p.triggerMode == TriggerMode::Gate)
    {
        envelope.noteOff();
        filterEnvelope.noteOff();
    }
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
    filterEnvelope.setParameters (p.fenvAttackMs, p.fenvDecayMs, p.fenvSustain, p.fenvReleaseMs);
    if (released && oneShot && p.freeze && ! envelope.isReleasing())
    {
        envelope.noteOff();
        filterEnvelope.noteOff();
    }

    // Glide (constant time) towards the target note.
    if (glideStep > 0.0)
    {
        const double delta = glideStep * n;
        if (std::abs (note - currentNote) <= delta) { currentNote = note; glideStep = 0.0; }
        else currentNote += (note > currentNote ? delta : -delta);
    }

    const int modeFade = static_cast<int> (kModeFadeSeconds * hostRate);
    const double baseIncrement = sourceIncrement (view.sampleRate, hostRate);
    const double semis = currentNote - p.rootNote + p.coarseTune + (p.fineTune + detuneCents - p.rootTuneCents) / 100.0
                         + ctx.pitchBendSemitones + ctx.lfo * p.lfoToPitch;
    const double ratio = std::pow (2.0, semis / 12.0);
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
        s.positionOffset = ctx.lfo * p.lfoToGrainPos * 0.25 * view.sampleRate;
        granular.render (view, region, playhead, granularMode, granularMode, s, bufL.data(), bufR.data(), n);
    }

    if (! granularMode) natural.render (view, region, playhead, naturalIncrement, bufL.data(), bufR.data(), n);
    if (! naturalTail.isSilent())
        naturalTail.render (view, region, tailPlayhead, tailIncrement, bufL.data(), bufR.data(), n);

    if (granularMode && p.playbackMode == PlaybackMode::Pitch)
        formant.process (bufL.data(), bufR.data(), n, ratio, ctx.formant);

    // Drive (soft saturation) before the filter, as on an analogue synth.
    if (p.drive > 0.001f)
    {
        const float gain = 1.0f + p.drive * p.drive * 24.0f;
        const float makeup = 1.0f / std::pow (gain, 0.6f);
        for (int i = 0; i < n; ++i)
        {
            bufL[(size_t) i] = dsp::driveSample (bufL[(size_t) i], gain, makeup);
            bufR[(size_t) i] = dsp::driveSample (bufR[(size_t) i], gain, makeup);
        }
    }

    // Per-voice filter: base cutoff + filter envelope + LFO + velocity.
    float fenv = 0.0f;
    for (int i = 0; i < n; ++i) fenv = filterEnvelope.next();
    float log2Cut = std::log2 (std::clamp (p.cutoffHz, 20.0f, 20000.0f))
                    + p.fenvAmount * 6.0f * fenv
                    + p.lfoToCutoff * 4.0f * ctx.lfo
                    + p.velToFilter * 4.0f * (velocity01 - 1.0f);
    log2Cut = std::clamp (log2Cut, 4.33f, 14.29f); // 20 Hz .. 20 kHz
    filter.setMode (static_cast<dsp::MultimodeFilter::Mode> (p.filterMode));
    filter.setCutoff (std::exp2 (log2Cut));
    filter.setResonance (p.resonance);
    filter.process (bufL.data(), bufR.data(), n);

    // Amp envelope, velocity, tremolo and (unison / LFO) pan, ramped across the chunk.
    const float amp = 1.0f - p.lfoToAmp * 0.5f * (1.0f - ctx.lfo);
    const float pan = std::clamp (panOffset + ctx.lfo * p.lfoToPan, -1.0f, 1.0f);
    const float panL = pan > 0.0f ? 1.0f - pan : 1.0f;
    const float panR = pan < 0.0f ? 1.0f + pan : 1.0f;
    if (firstChunk) { lastAmp = amp; lastPanL = panL; lastPanR = panR; firstChunk = false; }
    const float inv = 1.0f / static_cast<float> (n);
    const float dA = (amp - lastAmp) * inv, dL = (panL - lastPanL) * inv, dR = (panR - lastPanR) * inv;

    const float vg = velocityGain * kVoiceHeadroom * ctx.unisonGain;
    for (int i = 0; i < n; ++i)
    {
        lastAmp += dA; lastPanL += dL; lastPanR += dR;
        const float g = envelope.next() * vg * lastAmp;
        mixL[i] += bufL[(size_t) i] * g * lastPanL;
        mixR[i] += bufR[(size_t) i] * g * lastPanR;
    }
    lastAmp = amp; lastPanL = panL; lastPanR = panR;

    const bool sourceDone = playhead.ended && granular.getActiveGrainCount() == 0
                            && (naturalTail.isSilent() || tailPlayhead.ended);
    if (! envelope.isActive() || sourceDone) stopAndRelease();
}
} // namespace lhss
