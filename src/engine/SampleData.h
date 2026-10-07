#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <atomic>
#include <vector>

#include "../dsp/SampleReader.h"
#include "PitchDetector.h"

namespace lhss
{
/** Decoded audio + metadata. Built completely on a loader thread and never modified
    afterwards (immutable), so the audio thread can read it without locks.

    Ownership: instances are owned by std::shared_ptr inside SampleStore (and by the GUI for
    display). The audio thread only ever holds raw pointers; it registers each voice that uses
    a sample in `voiceRefs` so the store knows when an old sample can be freed safely. */
class SampleData
{
public:
    static constexpr int kOverviewBins = 2048;

    SampleData (juce::AudioBuffer<float>&& audio, double sourceSampleRate,
                juce::String displayName, juce::String filePath);

    int getNumFrames() const noexcept     { return audio.getNumSamples(); }
    int getNumChannels() const noexcept   { return audio.getNumChannels(); }
    double getSampleRate() const noexcept { return sampleRate; }
    double getDurationSeconds() const noexcept { return getNumFrames() / sampleRate; }
    const juce::String& getName() const noexcept { return name; }
    const juce::String& getPath() const noexcept { return path; }
    const float* getChannel (int ch) const noexcept { return audio.getReadPointer (juce::jmin (ch, getNumChannels() - 1)); }

    dsp::SourceView view() const noexcept;

    /** Precomputed min/max waveform overview (mono mix) for the GUI — generated at load time,
        never on the audio thread. */
    const std::vector<float>& getOverviewMin() const noexcept { return overviewMin; }
    const std::vector<float>& getOverviewMax() const noexcept { return overviewMax; }

    /** Fundamental pitch of the recording, analysed once at load time (see PitchDetector). */
    const PitchInfo& getPitch() const noexcept { return pitch; }

    /** Number of voices currently playing this sample (modified only by the audio thread). */
    mutable std::atomic<int> voiceRefs { 0 };

private:
    juce::AudioBuffer<float> audio;
    double sampleRate;
    juce::String name, path;
    std::vector<float> overviewMin, overviewMax;
    PitchInfo pitch;
};
} // namespace lhss
