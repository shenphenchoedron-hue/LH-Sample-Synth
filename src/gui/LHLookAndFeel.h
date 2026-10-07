#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace lhss::gui
{
namespace colours
{
inline const juce::Colour title     { 0xff13263d };
inline const juce::Colour subtitle  { 0xff7b91a8 };
inline const juce::Colour section   { 0xff18304c };
inline const juce::Colour label     { 0xff263c56 };
inline const juce::Colour value     { 0xff21354e };
inline const juce::Colour tiny      { 0xff63778f };
inline const juce::Colour blue      { 0xff1c72ff };
inline const juce::Colour green     { 0xff26d97b };
inline const juce::Colour orange    { 0xffff9d00 };
inline const juce::Colour pink      { 0xffff2f91 };
inline const juce::Colour purple    { 0xff8b2df5 };
inline const juce::Colour cyan      { 0xff20bde6 };
inline const juce::Colour outGreen  { 0xff39d34a };
inline const juce::Colour coral     { 0xffff5b78 };
inline const juce::Colour resOrange { 0xffff8b00 };
inline const juce::Colour buttonBg  { 0xfff5f9fc };
inline const juce::Colour buttonLine{ 0xffc8d6e2 };
inline const juce::Colour knobBody  { 0xffedf2f7 };
inline const juce::Colour knobLine  { 0xffc8d4df };
inline const juce::Colour pointer   { 0xff21364f };
inline const juce::Colour boxBg     { 0xfff8fbfd };
inline const juce::Colour boxLine   { 0xffd4dfe8 };
inline const juce::Colour switchOff { 0xffbcc8d3 };
inline const juce::Colour error     { 0xffe0344a };
} // namespace colours

juce::Font uiFont (float height, bool bold = true);

/** Light theme matching docs/ui-reference.svg: soft knobs with glowing accent arcs, pill
    switches, rounded segmented buttons. */
class LHLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    LHLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool highlighted, bool down) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool highlighted, bool down) override;
    juce::Font getTextButtonFont (juce::TextButton&, int) override { return uiFont (15.0f); }
    void drawComboBox (juce::Graphics&, int w, int h, bool down, int bx, int by, int bw, int bh, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override { return uiFont (15.0f); }
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
    juce::Font getPopupMenuFont() override { return uiFont (16.0f, false); }
};
} // namespace lhss::gui
