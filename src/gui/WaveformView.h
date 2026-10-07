#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <memory>

#include "../engine/SampleData.h"

namespace lhss::gui
{
/** Waveform display with draggable Sample Start / End and Loop Start / End markers.
    Draws the overview precomputed by SampleData at load time (never touches the audio
    thread). Marker drags write the parameters with proper host gestures. */
class WaveformView final : public juce::Component
{
public:
    explicit WaveformView (juce::AudioProcessorValueTreeState& state);

    void setSample (std::shared_ptr<const SampleData> sample);
    void setMessage (const juce::String& text, bool isError);
    /** Polled by the editor timer; repaints only when a marker parameter changed. */
    void refreshIfParametersChanged();

    void paint (juce::Graphics&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

    static constexpr int kLabelArea = 34;

private:
    enum Marker { SampleStart = 0, SampleEnd, LoopStart, LoopEnd, NumMarkers, None = -1 };

    juce::Rectangle<float> waveArea() const;
    float xForValue (float v) const;
    float valueForX (float x) const;
    float value (int m) const;
    int hitTest (float x) const;

    std::array<juce::RangedAudioParameter*, NumMarkers> params {};
    juce::RangedAudioParameter* loopOnParam = nullptr;
    std::array<float, NumMarkers + 1> lastValues {};
    std::shared_ptr<const SampleData> sample;
    juce::String message;
    bool messageIsError = false;
    int dragging = None, hover = None;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WaveformView)
};
} // namespace lhss::gui
