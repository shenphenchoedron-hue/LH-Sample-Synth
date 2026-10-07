#pragma once

#include <algorithm>
#include <cmath>

// Small header-only helpers shared by the Natural and Granular renderers so that both use
// exactly the same interpolation, loop-crossfade and boundary logic (no duplicated DSP).
// Everything here is allocation-free and safe to call on the audio thread.
namespace lhss::dsp
{
/** Non-owning, read-only view of decoded sample frames (1 or 2 channels). */
struct SourceView
{
    const float* left  = nullptr;
    const float* right = nullptr; // == left for mono sources
    int numFrames      = 0;
    double sampleRate  = 44100.0;

    bool isValid() const noexcept { return left != nullptr && numFrames > 1; }
};

/** Sample region and loop resolved to source frame positions (see engine/SampleRegion). */
struct ResolvedRegion
{
    double start = 0.0, end = 0.0;          // active region [start, end)
    bool loopOn = false;
    double loopStart = 0.0, loopEnd = 0.0;  // inside [start, end]
    double crossfade = 0.0;                 // frames, already clamped to legal size

    double loopLength() const noexcept { return loopEnd - loopStart; }
};

/** Per-voice traversal state of the source ("playhead"). */
struct PlayheadState
{
    double position = 0.0; // in source frames
    int direction   = 1;   // +1 forward, -1 reverse
    bool looping    = false;
    bool ended      = false;
};

/** 4-point, 3rd-order Hermite interpolation. Returns 0 outside the buffer. */
inline float readCubic (const float* data, int numFrames, double pos) noexcept
{
    if (pos < -1.0 || pos >= static_cast<double> (numFrames)) return 0.0f;

    const auto i = static_cast<int> (std::floor (pos));
    const auto t = static_cast<float> (pos - i);
    float xm1, x0, x1, x2;
    if (i >= 1 && i + 2 < numFrames)
    {
        xm1 = data[i - 1]; x0 = data[i]; x1 = data[i + 1]; x2 = data[i + 2];
    }
    else
    {
        auto at = [data, numFrames] (int k) noexcept { return (k >= 0 && k < numFrames) ? data[k] : 0.0f; };
        xm1 = at (i - 1); x0 = at (i); x1 = at (i + 1); x2 = at (i + 2);
    }
    const float c1 = 0.5f * (x1 - xm1);
    const float c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
    const float c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
    return ((c3 * t + c2) * t + c1) * t + x0;
}

/** sin(x·π/2) for x in [0, 1] (polynomial, max error < 0.1 %): equal-power fade gain. */
inline float equalPowerGain (float x) noexcept
{
    const float x2 = x * x;
    return x * (1.5707963f + x2 * (-0.6459640f + x2 * (0.0796926f - x2 * 0.0046818f)));
}

/** Reads one stereo frame. When the playhead is looping, the last `crossfade` frames before
    the loop boundary (in playback direction) are blended equal-power with the material that
    precedes the loop's other boundary, so the wrap is seamless. */
inline void readFrame (const SourceView& src, const ResolvedRegion& region, bool looping, int direction,
                       double pos, float& outL, float& outR) noexcept
{
    float l = readCubic (src.left, src.numFrames, pos);
    float r = (src.right == src.left) ? l : readCubic (src.right, src.numFrames, pos);

    if (looping && region.loopOn && region.crossfade >= 1.0)
    {
        const double xf = region.crossfade;
        double t = -1.0, other = 0.0;

        if (direction > 0 && pos >= region.loopEnd - xf && pos < region.loopEnd)
        {
            t = (pos - (region.loopEnd - xf)) / xf;
            other = pos - region.loopLength();
        }
        else if (direction < 0 && pos >= region.loopStart && pos < region.loopStart + xf)
        {
            t = (region.loopStart + xf - pos) / xf;
            other = pos + region.loopLength();
        }

        if (t >= 0.0)
        {
            const float gIn = equalPowerGain (static_cast<float> (t));
            const float gOut = equalPowerGain (1.0f - static_cast<float> (t));
            const float ol = readCubic (src.left, src.numFrames, other);
            const float orr = (src.right == src.left) ? ol : readCubic (src.right, src.numFrames, other);
            l = l * gOut + ol * gIn;
            r = r * gOut + orr * gIn;
        }
    }

    outL = l;
    outR = r;
}

/** Advances a playhead by `delta` frames (signed by its direction) honouring loop wraps and
    region end. Wrapping only happens when the boundary is actually crossed, so turning the loop
    on while the playhead is already past the loop simply lets it run out. */
inline void advancePlayhead (PlayheadState& ph, double delta, const ResolvedRegion& region) noexcept
{
    if (ph.ended) return;

    const double prev = ph.position;
    ph.position += delta * ph.direction;
    const bool canLoop = ph.looping && region.loopOn && region.loopLength() >= 1.0;

    if (ph.direction > 0)
    {
        if (canLoop && prev < region.loopEnd && ph.position >= region.loopEnd)
            while (ph.position >= region.loopEnd) ph.position -= region.loopLength();
        else if (ph.position >= region.end)
        {
            ph.position = region.end;
            ph.ended = true;
        }
    }
    else
    {
        if (canLoop && prev >= region.loopStart && ph.position < region.loopStart)
            while (ph.position < region.loopStart) ph.position += region.loopLength();
        else if (ph.position < region.start)
        {
            ph.position = region.start;
            ph.ended = true;
        }
    }
}

/** Frames left until the playhead leaves the region (infinite while looping inside the loop). */
inline double framesUntilEnd (const PlayheadState& ph, const ResolvedRegion& region) noexcept
{
    if (ph.looping && region.loopOn)
    {
        if (ph.direction > 0 && ph.position < region.loopEnd) return 1.0e12;
        if (ph.direction < 0 && ph.position >= region.loopStart) return 1.0e12;
    }
    return ph.direction > 0 ? region.end - ph.position : ph.position - region.start;
}
} // namespace lhss::dsp
