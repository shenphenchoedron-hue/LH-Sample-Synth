#pragma once

#include "../dsp/SampleReader.h"

namespace lhss
{
inline constexpr int kMinRegionFrames = 64;
inline constexpr int kMinLoopFrames   = 32;

/** Converts normalised region/loop parameters into legal source-frame positions:
    - all values are clamped to [0, 1]
    - Sample Start is always < Sample End (minimum length kMinRegionFrames)
    - loop bounds are clamped inside the active region and ordered
    - the loop crossfade is limited to half the loop length and to the material that is
      available before the loop start (forward) / after the loop end (reverse). */
dsp::ResolvedRegion resolveRegion (int numFrames, double sourceRate,
                                   float sampleStart, float sampleEnd,
                                   bool loopOn, float loopStart, float loopEnd,
                                   float crossfadeMs) noexcept;
} // namespace lhss
