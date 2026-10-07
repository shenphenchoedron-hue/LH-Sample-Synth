#include "Knob.h"
#include "LHLookAndFeel.h"

namespace lhss::gui
{
Knob::Knob (juce::AudioProcessorValueTreeState& state, const juce::String& paramID,
            const juce::String& t, juce::Colour accent)
    : title (t)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.22f, juce::MathConstants<float>::pi * 2.78f, true);
    slider.setColour (juce::Slider::rotarySliderFillColourId, accent);
    slider.setMouseDragSensitivity (220);
    slider.setVelocityBasedMode (false);
    slider.setPopupDisplayEnabled (false, false, nullptr);
    addAndMakeVisible (slider);

    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (state, paramID, slider);
    if (auto* p = state.getParameter (paramID))
        slider.setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()));
    slider.onValueChange = [this] { repaint(); };
    slider.setTooltip (t);
}

void Knob::resized()
{
    const int h = getHeight();
    const int dial = h - 16 - 26;
    slider.setBounds ((getWidth() - dial) / 2, 15, dial, dial);
}

void Knob::paint (juce::Graphics& g)
{
    g.setFont (uiFont (11.0f));
    g.setColour (colours::label);
    g.drawText (title, getLocalBounds().removeFromTop (14), juce::Justification::centred, false);

    const auto box = juce::Rectangle<float> ((getWidth() - 56) * 0.5f, getHeight() - 23.0f, 56.0f, 22.0f);
    g.setColour (colours::boxBg);
    g.fillRoundedRectangle (box, 6.0f);
    g.setColour (colours::boxLine);
    g.drawRoundedRectangle (box, 6.0f, 1.0f);
    g.setFont (uiFont (10.5f));
    g.setColour (colours::value);
    g.drawText (slider.getTextFromValue (slider.getValue()), box, juce::Justification::centred, false);
}

//==============================================================================
ChoiceSegment::ChoiceSegment (juce::RangedAudioParameter& param, const juce::StringArray& labels,
                              juce::Colour accent, juce::Array<int> buttonWidths, int g)
    : widths (std::move (buttonWidths)), gap (g), parameter (param),
      attachment (param, [this] (float v) { update (v); }, nullptr)
{
    for (int i = 0; i < labels.size(); ++i)
    {
        auto* b = buttons.add (new juce::TextButton (labels[i]));
        b->setClickingTogglesState (false);
        b->setColour (juce::TextButton::buttonOnColourId, accent);
        b->onClick = [this, i] { attachment.setValueAsCompleteGesture (static_cast<float> (i)); };
        addAndMakeVisible (b);
    }
    attachment.sendInitialUpdate();
}

void ChoiceSegment::update (float value)
{
    const int index = juce::roundToInt (value);
    for (int i = 0; i < buttons.size(); ++i)
        buttons[i]->setToggleState (i == index, juce::dontSendNotification);
}

void ChoiceSegment::resized()
{
    int x = 0;
    for (int i = 0; i < buttons.size(); ++i)
    {
        const int w = i < widths.size() ? widths[i] : 80;
        buttons[i]->setBounds (x, 0, w, getHeight());
        x += w + gap;
    }
}
} // namespace lhss::gui
