#pragma once

#include "SampleReader.h"

namespace lhss::dsp
{
/** Conventional resampling playback (Natural mode): the playhead advances by
    baseIncrement * pitchRatio, so pitch and speed change together. At the root note this is
    exactly the original recording (with host/source rate conversion via cubic interpolation).

    Also provides short gain ramps used when a voice switches between Natural and granular
    rendering (e.g. Freeze toggled mid-note), and an anti-click fade at the region end. */
class NaturalPlaybackProcessor
{
public:
    void reset() noexcept { gain = 1.0f; gainStep = 0.0f; targetGain = 1.0f; }
    void startFadeIn (int samples) noexcept;
    void startFadeOut (int samples) noexcept;
    bool isSilent() const noexcept { return gain <= 0.0f && targetGain <= 0.0f; }

    /** Adds n samples into outL/outR and advances `playhead`. */
    void render (const SourceView& src, const ResolvedRegion& region, PlayheadState& playhead,
                 double increment, float* outL, float* outR, int n) noexcept;

private:
    float gain = 1.0f, gainStep = 0.0f, targetGain = 1.0f;
};
} // namespace lhss::dsp
