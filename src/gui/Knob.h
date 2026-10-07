#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace lhss::gui
{
/** Title + rotary dial + value box, attached to one APVTS parameter. */
class Knob final : public juce::Component
{
public:
    Knob (juce::AudioProcessorValueTreeState& state, const juce::String& paramID,
          const juce::String& title, juce::Colour accent);

    juce::Slider& getSlider() noexcept { return slider; }

    static constexpr int kTitleHeight = 19;
    static constexpr int kBoxHeight = 27;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    juce::Slider slider;
    juce::String title;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Knob)
};

/** Row of mutually exclusive buttons bound to a choice parameter (e.g. Natural / Pitch). */
class ChoiceSegment final : public juce::Component
{
public:
    ChoiceSegment (juce::RangedAudioParameter& param, const juce::StringArray& labels,
                   juce::Colour accent, juce::Array<int> buttonWidths, int gap);
    void resized() override;

private:
    void update (float value);

    juce::OwnedArray<juce::TextButton> buttons;
    juce::Array<int> widths;
    int gap;
    juce::RangedAudioParameter& parameter;
    juce::ParameterAttachment attachment;
};
} // namespace lhss::gui
