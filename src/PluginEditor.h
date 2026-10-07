#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>

#include "PluginProcessor.h"
#include "gui/Knob.h"
#include "gui/LHLookAndFeel.h"
#include "gui/WaveformView.h"

/** Editor laid out on a fixed 2400 x 540 design canvas (matching docs/ui-reference.svg) that is
    uniformly scaled to the window size. All controls are visible at once — nothing is hidden
    in menus except the list-type selectors (root note, filter mode). */
class LHSampleSynthEditor final : public juce::AudioProcessorEditor,
                                  public juce::FileDragAndDropTarget,
                                  private juce::ChangeListener,
                                  private juce::Timer
{
public:
    explicit LHSampleSynthEditor (LHSampleSynthProcessor&);
    ~LHSampleSynthEditor() override;

    void resized() override;
    void paint (juce::Graphics&) override;

    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void filesDropped (const juce::StringArray& files, int, int) override;

    static constexpr int kDesignWidth = 2400;
    static constexpr int kDesignHeight = 910;                 // plugin layout (no keyboard)
    static constexpr int kKeyboardHeight = 210;               // extra height for the Standalone keyboard
    static constexpr int kKeyboardLowestNote = 21;            // A0
    static constexpr int kKeyboardHighestNote = 108;          // C8 (88 keys)

    /** True when the on-screen 88-key keyboard is shown (Standalone app only). */
    bool hasKeyboard() const noexcept { return keyboard != nullptr; }
    int getDesignHeight() const noexcept { return kDesignHeight + (hasKeyboard() ? kKeyboardHeight : 0); }

private:
    class Canvas final : public juce::Component
    {
    public:
        void paint (juce::Graphics&) override;
        juce::String fileName, fileInfo, footer, pitchName, pitchInfo;
        juce::Image logo;
        bool fileError = false;
        int designHeight = kDesignHeight;
        bool showKeyboard = false;
    };

    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void timerCallback() override;
    void openFileChooser();
    void refreshSampleInfo();
    void layoutCanvas();

    LHSampleSynthProcessor& processor;
    lhss::gui::LHLookAndFeel lookAndFeel;
    Canvas canvas;

    juce::TextButton loadButton { "Load Sample" };
    juce::TextButton resetButton { "Reset to Default" };
    juce::TextButton tuneButton { "Tune to Pitch" };
    juce::ComboBox rootNoteBox;
    juce::ParameterAttachment rootNoteAttachment;
    lhss::gui::ChoiceSegment playbackMode, triggerMode;
    juce::ToggleButton reverseSwitch, freezeSwitch, loopSwitch;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> reverseAttach, freezeAttach, loopAttach;
    juce::ComboBox filterModeBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> filterModeAttach;

    lhss::gui::WaveformView waveform;

    lhss::gui::Knob sampleStart, sampleEnd;
    lhss::gui::Knob loopStart, loopEnd, loopCrossfade;
    lhss::gui::Knob attack, decay, sustain, release, velocity, polyphony;
    lhss::gui::Knob grainSize, grainDensity, posRandom, pitchRandom, stereoSpread, formant;
    lhss::gui::Knob cutoff, resonance;
    lhss::gui::Knob pan, width, outputGain;

    // Synth section (second row)
    juce::ComboBox lfoShapeBox, lfoDivisionBox, delayDivisionBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> lfoShapeAttach, lfoDivisionAttach, delayDivisionAttach;
    juce::ToggleButton lfoOnSwitch, lfoSyncSwitch, delaySyncSwitch, pingPongSwitch;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> lfoOnAttach, lfoSyncAttach, delaySyncAttach, pingPongAttach;
    lhss::gui::ChoiceSegment voiceMode;
    lhss::gui::Knob lfoRate, lfoToPitch, lfoToCutoff, lfoToAmp, lfoToPan, lfoToGrainPos;
    lhss::gui::Knob fenvAttack, fenvDecay, fenvSustain, fenvRelease, fenvAmount, velToFilter;
    lhss::gui::Knob glide, coarseTune, fineTune, unisonVoices, unisonDetune, drive;
    lhss::gui::Knob chorusRate, chorusDepth, chorusMix, delayTime, delayFeedback, delayMix;
    lhss::gui::Knob reverbSize, reverbDamping, reverbMix;

    std::unique_ptr<juce::MidiKeyboardComponent> keyboard;   // Standalone only

    std::unique_ptr<juce::FileChooser> chooser;
    double shownDuration = -1.0;
    int shownVoices = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LHSampleSynthEditor)
};
