#pragma once

#include "SampleReader.h"

namespace lhss::dsp
{
/** One grain: a short Hann-windowed excerpt of the source read at its own playback rate.
    Grains are plain data stored in a fixed, preallocated pool per voice (no allocation). */
struct Grain
{
    bool active = false;
    bool wrapInLoop = false;   // grain was born inside a loop and wraps with it
    int direction = 1;
    double position = 0.0;     // source frame
    double increment = 1.0;    // source frames per output sample before the voice pitch ratio (sign via direction)
    int length = 0;            // output samples
    int age = 0;
    float gainL = 1.0f, gainR = 1.0f;

    /** Renders up to n output samples, adding into outL/outR scaled by `gain`. `window` is a
        Hann table of tableSize+1 points. Returns false once the grain has finished. */
    bool render (const SourceView& src, const ResolvedRegion& region, const float* window, int tableSize,
                 float gain, double pitchRatio, float* outL, float* outR, int n) noexcept;
};
} // namespace lhss::dsp
