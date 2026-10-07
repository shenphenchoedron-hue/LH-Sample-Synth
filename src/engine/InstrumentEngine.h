#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <atomic>
#include <vector>

#include "EngineParams.h"
#include "VoiceManager.h"
#include "../dsp/MultimodeFilter.h"
#include "../dsp/ParameterSmoother.h"

namespace lhss
{
/** The complete instrument DSP, independent of any plugin format (VST3/AU/CLAP wrappers all
    drive this same object through PluginProcessor).

    Realtime contract for process(): no allocation, no locks, no I/O, no logging. All buffers
    are sized in prepare(). Host blocks of any size are handled (internally split into chunks
    of the prepared size) and MIDI events are applied at their exact sample offsets. */
class InstrumentEngine
{
public:
    void prepare (double hostSampleRate, int maxBlockSize);
    void reset() noexcept;

    /** Audio thread, once per block before process(). */
    void setParameters (const EngineParams& p) noexcept;

    /** Any non-audio thread: hand over a new immutable sample (see SampleStore). */
    void setPendingSample (const SampleData* sample) noexcept { pending.store (sample, std::memory_order_release); }
    /** The newest sample the audio thread has adopted for new voices. */
    const SampleData* getAcknowledgedSample() const noexcept { return acknowledged.load (std::memory_order_acquire); }

    void process (juce::AudioBuffer<float>& buffer, const juce::MidiBuffer& midi) noexcept;

    int getActiveVoiceCount() const noexcept { return voiceManager.getActiveVoiceCount(); }
    const VoiceManager& getVoiceManager() const noexcept { return voiceManager; }
    double getSampleRate() const noexcept { return sampleRate; }

    /** Applies one raw MIDI message immediately (used by process(); exposed for tests). */
    void handleMidi (const juce::uint8* data, int numBytes) noexcept;

private:
    void adoptPendingSample() noexcept;
    void renderRange (juce::AudioBuffer<float>& buffer, int start, int numSamples) noexcept;
    void renderChunk (int numSamples) noexcept;

    std::atomic<const SampleData*> pending { nullptr };
    std::atomic<const SampleData*> acknowledged { nullptr };
    const SampleData* current = nullptr; // audio-thread only

    EngineParams params;
    VoiceManager voiceManager;
    dsp::MultimodeFilter filter;
    dsp::ParameterSmoother gainSmoother, panSmoother, widthSmoother, formantSmoother, grainSizeSmoother;

    std::vector<float> mixL, mixR;
    double sampleRate = 44100.0;
    int maxBlock = 0;
    double pitchBendRatio = 1.0;
};
} // namespace lhss
