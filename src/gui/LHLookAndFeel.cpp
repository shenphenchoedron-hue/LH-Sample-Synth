#include "LHLookAndFeel.h"

namespace lhss::gui
{
juce::Font uiFont (float height, bool bold)
{
    return juce::Font (juce::FontOptions ("Arial", height, bold ? juce::Font::bold : juce::Font::plain));
}

LHLookAndFeel::LHLookAndFeel()
{
    setColour (juce::PopupMenu::backgroundColourId, juce::Colours::white);
    setColour (juce::PopupMenu::textColourId, colours::label);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, colours::blue);
    setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
    setColour (juce::ComboBox::textColourId, juce::Colour (0xff2b3c50));
    setColour (juce::ComboBox::backgroundColourId, colours::buttonBg);
    setColour (juce::ComboBox::outlineColourId, colours::buttonLine);
    setColour (juce::ComboBox::arrowColourId, colours::tiny);
    setColour (juce::TextButton::textColourOffId, juce::Colour (0xff2b3c50));
    setColour (juce::TextButton::textColourOnId, juce::Colours::white);
    setColour (juce::TextButton::buttonColourId, colours::buttonBg);
}

void LHLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                      float startAngle, float endAngle, juce::Slider& slider)
{
    const auto bounds = juce::Rectangle<int> (x, y, w, h).toFloat();
    const auto centre = bounds.getCentre();
    const float arcRadius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f - 5.0f;
    const float bodyRadius = arcRadius - 7.0f;
    const auto accent = slider.findColour (juce::Slider::rotarySliderFillColourId);
    const float angle = startAngle + pos * (endAngle - startAngle);

    // Body
    g.setColour (colours::knobBody);
    g.fillEllipse (centre.x - bodyRadius, centre.y - bodyRadius, bodyRadius * 2.0f, bodyRadius * 2.0f);
    g.setColour (colours::knobLine);
    g.drawEllipse (centre.x - bodyRadius, centre.y - bodyRadius, bodyRadius * 2.0f, bodyRadius * 2.0f, 2.0f);

    // Faint track
    juce::Path track;
    track.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f, startAngle, endAngle, true);
    g.setColour (colours::knobLine.withAlpha (0.35f));
    g.strokePath (track, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Accent arc with glow
    if (pos > 0.001f)
    {
        juce::Path arc;
        arc.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f, startAngle, angle, true);
        g.setColour (accent.withAlpha (0.18f));
        g.strokePath (arc, juce::PathStrokeType (13.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (accent);
        g.strokePath (arc, juce::PathStrokeType (7.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // Pointer
    const float len = bodyRadius * 0.62f;
    const juce::Point<float> tip (centre.x + len * std::sin (angle), centre.y - len * std::cos (angle));
    g.setColour (colours::pointer);
    g.drawLine ({ centre, tip }, 3.0f);
}

void LHLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool highlighted, bool)
{
    const auto r = b.getLocalBounds().toFloat().reduced (1.0f);
    const bool on = b.getToggleState();
    const auto accent = b.findColour (juce::ToggleButton::tickColourId);

    g.setColour (on ? accent : colours::switchOff);
    g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
    if (highlighted)
    {
        g.setColour (juce::Colours::white.withAlpha (0.15f));
        g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
    }

    const float d = r.getHeight() - 6.0f;
    const float cx = on ? r.getRight() - 3.0f - d : r.getX() + 3.0f;
    g.setColour (juce::Colours::white);
    g.fillEllipse (cx, r.getY() + 3.0f, d, d);
}

void LHLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool highlighted, bool down)
{
    const auto r = b.getLocalBounds().toFloat().reduced (1.0f);
    const bool on = b.getToggleState();
    const auto accent = b.findColour (juce::TextButton::buttonOnColourId);

    if (on)
    {
        juce::Path p;
        p.addRoundedRectangle (r, 9.0f);
        juce::DropShadow (accent.withAlpha (0.55f), 8, {}).drawForPath (g, p);
        g.setColour (accent);
        g.fillPath (p);
    }
    else
    {
        g.setColour (down ? colours::buttonLine : (highlighted ? juce::Colour (0xffeaf1f7) : colours::buttonBg));
        g.fillRoundedRectangle (r, 9.0f);
        g.setColour (colours::buttonLine);
        g.drawRoundedRectangle (r, 9.0f, 1.0f);
    }
}

void LHLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool)
{
    g.setFont (uiFont (11.0f));
    g.setColour (b.getToggleState() ? juce::Colours::white : juce::Colour (0xff2b3c50));
    g.drawText (b.getButtonText(), b.getLocalBounds(), juce::Justification::centred, false);
}

void LHLookAndFeel::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box)
{
    const auto r = juce::Rectangle<int> (0, 0, w, h).toFloat().reduced (1.0f);
    g.setColour (colours::buttonBg);
    g.fillRoundedRectangle (r, 9.0f);
    g.setColour (box.hasKeyboardFocus (true) ? colours::blue.withAlpha (0.6f) : colours::buttonLine);
    g.drawRoundedRectangle (r, 9.0f, 1.0f);

    juce::Path arrow;
    const float ax = r.getRight() - 14.0f, ay = r.getCentreY();
    arrow.addTriangle (ax - 4.0f, ay - 2.0f, ax + 4.0f, ay - 2.0f, ax, ay + 3.0f);
    g.setColour (colours::tiny);
    g.fillPath (arrow);
}

void LHLookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (box.getLocalBounds().withTrimmedRight (14));
    label.setFont (uiFont (11.0f));
    label.setJustificationType (juce::Justification::centred);
}
} // namespace lhss::gui
