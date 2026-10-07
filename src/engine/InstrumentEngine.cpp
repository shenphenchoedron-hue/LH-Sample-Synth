#include "InstrumentEngine.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace lhss
{
namespace
{
/** Transparent below -1 dBFS, smooth tanh knee above, never exceeds 0 dBFS. Prevents spikes
    from many stacked voices without compressing normal material. */
inline float softLimit (float x) noexcept
{
    constexpr float t = 0.891f;
    const float a = std::abs (x);
    if (a <= t) return x;
    const float y = t + (1.0f - t) * std::tanh ((a - t) / (1.0f - t));
    return x < 0.0f ? -y : y;
}
} // namespace

void InstrumentEngine::prepare (double hostSampleRate, int maxBlockSize)
{
    sampleRate = hostSampleRate > 0.0 ? hostSampleRate : 44100.0;
    maxBlock = std::max (32, maxBlockSize);
    mixL.assign ((size_t) maxBlock, 0.0f);
    mixR.assign ((size_t) maxBlock, 0.0f);

    voiceManager.prepare (sampleRate, maxBlock);
    filter.prepare (sampleRate);

    gainSmoother.reset (sampleRate, 0.03);
    panSmoother.reset (sampleRate, 0.03);
    widthSmoother.reset (sampleRate, 0.03);
    formantSmoother.reset (sampleRate, 0.05);
    grainSizeSmoother.reset (sampleRate, 0.05);

    gainSmoother.setCurrentAndTarget (juce::Decibels::decibelsToGain (params.outputGainDb));
    panSmoother.setCurrentAndTarget (params.pan);
    widthSmoother.setCurrentAndTarget (params.stereoWidth);
    formantSmoother.setCurrentAndTarget (params.formant);
    grainSizeSmoother.setCurrentAndTarget (params.grainSizeMs);
    filter.setCutoff (params.cutoffHz);
    filter.setResonance (params.resonance);
}

void InstrumentEngine::reset() noexcept
{
    voiceManager.stopAll();
    filter.reset();
    pitchBendRatio = 1.0;
}

void InstrumentEngine::setParameters (const EngineParams& p) noexcept
{
    params = p;
    params.polyphony = std::clamp (p.polyphony, 1, kMaxPolyphony);
    params.rootNote = std::clamp (p.rootNote, 0, 127);

    gainSmoother.setTarget (juce::Decibels::decibelsToGain (std::clamp (p.outputGainDb, -48.0f, 12.0f), -48.0f));
    panSmoother.setTarget (std::clamp (p.pan, -1.0f, 1.0f));
    widthSmoother.setTarget (std::clamp (p.stereoWidth, 0.0f, 2.0f));
    formantSmoother.setTarget (std::clamp (p.formant, -1.0f, 1.0f));
    grainSizeSmoother.setTarget (std::clamp (p.grainSizeMs, 10.0f, 500.0f));
    filter.setMode (static_cast<dsp::MultimodeFilter::Mode> (p.filterMode));
    filter.setCutoff (p.cutoffHz);
    filter.setResonance (p.resonance);
}

void InstrumentEngine::adoptPendingSample() noexcept
{
    if (const auto* next = pending.exchange (nullptr, std::memory_order_acq_rel))
    {
        current = next; // new voices use it; sounding voices keep their own sample
        acknowledged.store (next, std::memory_order_release);
    }
}

void InstrumentEngine::handleMidi (const juce::uint8* d, int numBytes) noexcept
{
    if (numBytes < 1) return;
    const int status = d[0] & 0xF0;
    const int d1 = numBytes > 1 ? d[1] & 0x7F : 0;
    const int d2 = numBytes > 2 ? d[2] & 0x7F : 0;

    switch (status)
    {
        case 0x90:
            if (d2 > 0) { voiceManager.noteOn (current, d1, d2 / 127.0f, params); break; }
            [[fallthrough]];
        case 0x80:
            voiceManager.noteOff (d1, params);
            break;
        case 0xB0:
            if (d1 == 123) voiceManager.allNotesOff (params);       // All Notes Off
            else if (d1 == 120) voiceManager.killAll();              // All Sound Off
            break;
        case 0xE0:
        {
            const int bend = (d2 << 7 | d1) - 8192;                  // ±2 semitones
            pitchBendRatio = std::pow (2.0, (bend / 8192.0) * 2.0 / 12.0);
            break;
        }
        default: break;
    }
}

void InstrumentEngine::process (juce::AudioBuffer<float>& buffer, const juce::MidiBuffer& midi) noexcept
{
    buffer.clear();
    adoptPendingSample();
    if (maxBlock == 0) return;

    const int total = buffer.getNumSamples();
    int cursor = 0;
    for (const auto meta : midi)
    {
        const int pos = std::clamp (meta.samplePosition, 0, total);
        if (pos > cursor)
        {
            renderRange (buffer, cursor, pos - cursor);
            cursor = pos;
        }
        handleMidi (meta.data, meta.numBytes);
    }
    if (cursor < total) renderRange (buffer, cursor, total - cursor);
}

void InstrumentEngine::renderRange (juce::AudioBuffer<float>& buffer, int start, int numSamples) noexcept
{
    const int outChans = buffer.getNumChannels();
    while (numSamples > 0)
    {
        const int n = std::min (numSamples, maxBlock);
        renderChunk (n);

        if (outChans >= 2)
        {
            buffer.copyFrom (0, start, mixL.data(), n);
            buffer.copyFrom (1, start, mixR.data(), n);
        }
        else if (outChans == 1)
        {
            float* out = buffer.getWritePointer (0, start);
            for (int i = 0; i < n; ++i) out[i] = 0.5f * (mixL[(size_t) i] + mixR[(size_t) i]);
        }
        start += n;
        numSamples -= n;
    }
}

void InstrumentEngine::renderChunk (int n) noexcept
{
    std::memset (mixL.data(), 0, sizeof (float) * (size_t) n);
    std::memset (mixR.data(), 0, sizeof (float) * (size_t) n);

    VoiceContext ctx;
    ctx.params = &params;
    ctx.hostRate = sampleRate;
    ctx.pitchBendRatio = pitchBendRatio;
    ctx.formant = formantSmoother.skip (n);
    ctx.grainSizeMs = grainSizeSmoother.skip (n);
    voiceManager.render (ctx, mixL.data(), mixR.data(), n);

    filter.process (mixL.data(), mixR.data(), n);

    for (int i = 0; i < n; ++i)
    {
        const float width = widthSmoother.next();
        const float pan = panSmoother.next();
        const float gain = gainSmoother.next();

        float l = mixL[(size_t) i], r = mixR[(size_t) i];
        const float mid = 0.5f * (l + r);
        const float side = 0.5f * (l - r) * width;
        l = mid + side;
        r = mid - side;

        // Balance-style pan: unity in the centre, attenuates the opposite side.
        l *= pan > 0.0f ? 1.0f - pan : 1.0f;
        r *= pan < 0.0f ? 1.0f + pan : 1.0f;

        mixL[(size_t) i] = softLimit (l * gain);
        mixR[(size_t) i] = softLimit (r * gain);
    }
}
} // namespace lhss
