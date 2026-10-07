#include "PluginEditor.h"
#include "parameters/ParameterIDs.h"
#include "parameters/ParameterLayout.h"

namespace ids = lhss::ids;
namespace col = lhss::gui::colours;
using lhss::gui::uiFont;

namespace
{
struct Panel
{
    int x, w;
    juce::Colour colour;
    const char* title;
};

const Panel panels[] = {
    { 38,   520, col::green,    "Sample" },
    { 570,  292, col::orange,   "Loop" },
    { 874,  420, col::pink,     "Envelope (ADSR)" },
    { 1306, 604, col::purple,   "Granular" },
    { 1922, 230, col::cyan,     "Filter" },
    { 2164, 198, col::outGreen, "Output" },
};

void drawLabel (juce::Graphics& g, const juce::String& text, int x, int baselineY, int width = 140,
                juce::Justification j = juce::Justification::left)
{
    g.setFont (uiFont (11.0f));
    g.setColour (col::label);
    g.drawText (text, x, baselineY - 11, width, 14, j, false);
}

/** Places a knob centred on cx with its title starting at titleBaseline (design coords). */
void placeKnob (juce::Component& k, int cx, int titleBaseline, int width = 92, int height = 116)
{
    k.setBounds (cx - width / 2, titleBaseline - 11, width, height);
}
} // namespace

//==============================================================================
void LHSampleSynthEditor::Canvas::paint (juce::Graphics& g)
{
    const auto full = juce::Rectangle<float> (0, 0, (float) kDesignWidth, (float) kDesignHeight);
    g.fillAll (juce::Colours::white);

    const auto frame = juce::Rectangle<float> (18, 20, 2364, 500);
    juce::Path framePath;
    framePath.addRoundedRectangle (frame, 24.0f);
    juce::DropShadow (juce::Colour (0xff58708a).withAlpha (0.18f), 28, { 0, 8 }).drawForPath (g, framePath);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xfffbfdff), 0, frame.getY(),
                                             juce::Colour (0xffeef4f8), 0, frame.getBottom(), false));
    g.fillPath (framePath);
    g.setColour (juce::Colour (0xffd4e1ec));
    g.strokePath (framePath, juce::PathStrokeType (2.0f));
    juce::ignoreUnused (full);

    // Title
    juce::AttributedString title;
    title.append ("LH ", uiFont (34.0f), col::title);
    title.append ("Sample", uiFont (34.0f), col::blue);
    title.append (" Synth", uiFont (34.0f), col::title);
    title.draw (g, { 48, 34, 290, 42 });
    g.setFont (uiFont (12.0f));
    g.setColour (col::subtitle);
    g.drawText ("R E C O R D E D   S O U N D   I N S T R U M E N T", 50, 77, 300, 14, juce::Justification::left, false);

    // File info
    g.setFont (uiFont (11.0f));
    g.setColour (fileError ? col::error : col::label);
    g.drawText (fileName, 500, 44, 270, 14, juce::Justification::left, true);
    g.setFont (uiFont (10.0f, false));
    g.setColour (col::tiny);
    g.drawText (fileInfo, 500, 66, 270, 14, juce::Justification::left, true);

    drawLabel (g, "Root Note", 790, 41);
    drawLabel (g, "Playback Mode", 890, 41);
    drawLabel (g, "Trigger Mode", 1070, 41);
    drawLabel (g, "Reverse", 1260, 42);
    drawLabel (g, "Freeze", 1340, 42);

    // Section panels
    for (const auto& p : panels)
    {
        const auto r = juce::Rectangle<float> ((float) p.x, 112.0f, (float) p.w, 352.0f);
        g.setColour (juce::Colours::white.withAlpha (0.76f));
        g.fillRoundedRectangle (r, 15.0f);
        {
            juce::Graphics::ScopedSaveState s (g);
            g.reduceClipRegion (r.withHeight (39.0f).toNearestInt());
            g.setColour (p.colour.withAlpha (0.08f));
            g.fillRoundedRectangle (r, 15.0f);
        }
        g.setColour (p.colour.withAlpha (0.42f));
        g.drawRoundedRectangle (r, 15.0f, 1.0f);
        g.setColour (p.colour);
        g.fillRoundedRectangle ((float) p.x + 12.0f, 122.0f, 6.0f, 19.0f, 3.0f);
        g.setFont (uiFont (15.0f));
        g.setColour (col::section);
        g.drawText (p.title, p.x + 28, 128, p.w - 40, 16, juce::Justification::left, false);
    }

    drawLabel (g, "Loop On", 594, 166);
    drawLabel (g, "Mode", 1952, 177);

    g.setFont (uiFont (10.0f, false));
    g.setColour (col::tiny);
    g.drawText (footer, 48, 490, 1200, 14, juce::Justification::left, false);
}

//==============================================================================
LHSampleSynthEditor::LHSampleSynthEditor (LHSampleSynthProcessor& p)
    : AudioProcessorEditor (&p),
      processor (p),
      rootNoteAttachment (*p.getAPVTS().getParameter (ids::rootNote),
                          [this] (float v) { rootNoteBox.setSelectedId (juce::roundToInt (v) + 1, juce::dontSendNotification); },
                          nullptr),
      playbackMode (*p.getAPVTS().getParameter (ids::playbackMode), { "Natural", "Pitch" }, col::blue, { 88, 74 }, 5),
      triggerMode (*p.getAPVTS().getParameter (ids::triggerMode), { "One Shot", "Gate" }, col::orange, { 96, 74 }, 5),
      waveform (p.getAPVTS()),
      sampleStart (p.getAPVTS(), ids::sampleStart, "Sample Start", col::green),
      sampleEnd (p.getAPVTS(), ids::sampleEnd, "Sample End", col::blue),
      loopStart (p.getAPVTS(), ids::loopStart, "Loop Start", col::orange),
      loopEnd (p.getAPVTS(), ids::loopEnd, "Loop End", col::orange),
      loopCrossfade (p.getAPVTS(), ids::loopCrossfade, "Crossfade", col::orange),
      attack (p.getAPVTS(), ids::attack, "Attack", col::pink),
      decay (p.getAPVTS(), ids::decay, "Decay", col::pink),
      sustain (p.getAPVTS(), ids::sustain, "Sustain", col::pink),
      release (p.getAPVTS(), ids::release, "Release", col::pink),
      velocity (p.getAPVTS(), ids::velocitySens, "Velocity", col::coral),
      polyphony (p.getAPVTS(), ids::polyphony, "Polyphony", col::cyan),
      grainSize (p.getAPVTS(), ids::grainSize, "Grain Size", col::purple),
      grainDensity (p.getAPVTS(), ids::grainDensity, "Density", col::purple),
      posRandom (p.getAPVTS(), ids::grainPosRandom, "Position Rand", col::purple),
      pitchRandom (p.getAPVTS(), ids::pitchRandom, "Pitch Rand", col::purple),
      stereoSpread (p.getAPVTS(), ids::stereoSpread, "Stereo Spread", col::purple),
      formant (p.getAPVTS(), ids::formant, "Formant", col::purple),
      cutoff (p.getAPVTS(), ids::cutoff, "Cutoff", col::cyan),
      resonance (p.getAPVTS(), ids::resonance, "Resonance", col::resOrange),
      pan (p.getAPVTS(), ids::pan, "Pan", col::outGreen),
      width (p.getAPVTS(), ids::stereoWidth, "Stereo Width", col::outGreen),
      outputGain (p.getAPVTS(), ids::outputGain, "Output Gain", col::outGreen)
{
    setLookAndFeel (&lookAndFeel);
    addAndMakeVisible (canvas);

    loadButton.onClick = [this] { openFileChooser(); };
    canvas.addAndMakeVisible (loadButton);

    for (int n = 0; n < 128; ++n) rootNoteBox.addItem (lhss::midiNoteName (n), n + 1);
    rootNoteBox.onChange = [this] { rootNoteAttachment.setValueAsCompleteGesture ((float) (rootNoteBox.getSelectedId() - 1)); };
    rootNoteAttachment.sendInitialUpdate();
    canvas.addAndMakeVisible (rootNoteBox);

    canvas.addAndMakeVisible (playbackMode);
    canvas.addAndMakeVisible (triggerMode);

    auto setupSwitch = [this] (juce::ToggleButton& b, juce::Colour c, const char* id,
                               std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>& att)
    {
        b.setColour (juce::ToggleButton::tickColourId, c);
        b.setTooltip (processor.getAPVTS().getParameter (id)->getName (64));
        canvas.addAndMakeVisible (b);
        att = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (processor.getAPVTS(), id, b);
    };
    setupSwitch (reverseSwitch, col::purple, ids::reverse, reverseAttach);
    setupSwitch (freezeSwitch, col::cyan, ids::freeze, freezeAttach);
    setupSwitch (loopSwitch, col::orange, ids::loopOn, loopAttach);

    filterModeBox.addItemList ({ "Low Pass", "High Pass", "Band Pass" }, 1);
    canvas.addAndMakeVisible (filterModeBox);
    filterModeAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (processor.getAPVTS(), ids::filterMode, filterModeBox);

    canvas.addAndMakeVisible (waveform);
    for (auto* k : { &sampleStart, &sampleEnd, &loopStart, &loopEnd, &loopCrossfade, &attack, &decay, &sustain,
                     &release, &velocity, &polyphony, &grainSize, &grainDensity, &posRandom, &pitchRandom,
                     &stereoSpread, &formant, &cutoff, &resonance, &pan, &width, &outputGain })
        canvas.addAndMakeVisible (k);

    layoutCanvas();
    processor.getSampleBroadcaster().addChangeListener (this);
    refreshSampleInfo();

    setResizable (true, true);
    setResizeLimits (kDesignWidth / 2, kDesignHeight / 2, kDesignWidth * 3 / 2, kDesignHeight * 3 / 2);
    if (auto* c = getConstrainer()) c->setFixedAspectRatio ((double) kDesignWidth / kDesignHeight);
    setSize (kDesignWidth * 7 / 10, kDesignHeight * 7 / 10);
    startTimerHz (30);
}

LHSampleSynthEditor::~LHSampleSynthEditor()
{
    processor.getSampleBroadcaster().removeChangeListener (this);
    setLookAndFeel (nullptr);
}

void LHSampleSynthEditor::layoutCanvas()
{
    canvas.setBounds (0, 0, kDesignWidth, kDesignHeight);

    loadButton.setBounds (330, 39, 150, 34);
    rootNoteBox.setBounds (780, 49, 92, 34);
    playbackMode.setBounds (885, 49, 167, 34);
    triggerMode.setBounds (1065, 49, 175, 34);
    reverseSwitch.setBounds (1260, 52, 54, 26);
    freezeSwitch.setBounds (1340, 52, 54, 26);

    waveform.setBounds (58, 182 - lhss::gui::WaveformView::kLabelArea, 480, 165 + lhss::gui::WaveformView::kLabelArea + 6);
    placeKnob (sampleStart, 150, 365);
    placeKnob (sampleEnd, 355, 365);

    loopSwitch.setBounds (594, 176, 54, 26);
    placeKnob (loopStart, 628, 237, 88, 112);
    placeKnob (loopEnd, 725, 237, 88, 112);
    placeKnob (loopCrossfade, 812, 237, 86, 112);

    placeKnob (attack, 925, 207);
    placeKnob (decay, 1015, 207);
    placeKnob (sustain, 1105, 207);
    placeKnob (release, 1195, 207);
    placeKnob (velocity, 970, 353, 92, 106);
    placeKnob (polyphony, 1138, 353, 92, 106);

    const int grainX[] = { 1360, 1455, 1550, 1645, 1740, 1835 };
    juce::Component* grainKnobs[] = { &grainSize, &grainDensity, &posRandom, &pitchRandom, &stereoSpread, &formant };
    for (int i = 0; i < 6; ++i) placeKnob (*grainKnobs[i], grainX[i], 213);

    filterModeBox.setBounds (1942, 186, 185, 34);
    placeKnob (cutoff, 1977, 263);
    placeKnob (resonance, 2094, 263);

    placeKnob (pan, 2210, 189, 92, 112);
    placeKnob (width, 2315, 189, 92, 112);
    placeKnob (outputGain, 2262, 341, 92, 112);
}

void LHSampleSynthEditor::paint (juce::Graphics& g) { g.fillAll (juce::Colours::white); }

void LHSampleSynthEditor::resized()
{
    const float scale = juce::jmin ((float) getWidth() / kDesignWidth, (float) getHeight() / kDesignHeight);
    canvas.setTransform (juce::AffineTransform::scale (scale));
}

//==============================================================================
void LHSampleSynthEditor::openFileChooser()
{
    const auto status = processor.getSampleStatus();
    const auto start = status.path.isNotEmpty() ? juce::File (status.path).getParentDirectory()
                                                : juce::File::getSpecialLocation (juce::File::userMusicDirectory);
    chooser = std::make_unique<juce::FileChooser> ("Load a recording", start, lhss::SampleLoader::supportedWildcard());
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [this] (const juce::FileChooser& fc)
                          {
                              const auto file = fc.getResult();
                              if (file.existsAsFile()) processor.loadSampleAsync (file);
                          });
}

bool LHSampleSynthEditor::isInterestedInFileDrag (const juce::StringArray& files)
{
    return files.size() == 1 && juce::File (files[0]).hasFileExtension ("wav;wave;aif;aiff;aifc;flac;ogg");
}

void LHSampleSynthEditor::filesDropped (const juce::StringArray& files, int, int)
{
    if (! files.isEmpty()) processor.loadSampleAsync (juce::File (files[0]));
}

void LHSampleSynthEditor::changeListenerCallback (juce::ChangeBroadcaster*) { refreshSampleInfo(); }

void LHSampleSynthEditor::refreshSampleInfo()
{
    using State = LHSampleSynthProcessor::SampleState;
    const auto status = processor.getSampleStatus();
    const auto sample = processor.getLoadedSample();
    const auto fileName = juce::File (status.path).getFileName();

    canvas.fileError = false;
    waveform.setMessage ({}, false);

    switch (status.state)
    {
        case State::None:
            canvas.fileName = "No sample loaded";
            break;
        case State::Loading:
            canvas.fileName = "Loading " + fileName + "...";
            break;
        case State::Loaded:
            canvas.fileName = fileName;
            break;
        case State::Missing:
            canvas.fileName = "Missing: " + fileName;
            canvas.fileError = true;
            waveform.setMessage ("Sample missing:\n" + status.path + "\nParameters were restored - load the file again.", true);
            break;
        case State::Error:
            canvas.fileName = "Error: " + fileName;
            canvas.fileError = true;
            waveform.setMessage (status.message, true);
            break;
    }

    const bool showSample = sample != nullptr && status.state != State::Missing;
    waveform.setSample (showSample ? sample : nullptr);

    double duration = 0.0;
    if (showSample)
    {
        duration = sample->getDurationSeconds();
        canvas.fileInfo = juce::String (duration, 2) + " s  |  " + juce::String (sample->getSampleRate() / 1000.0, 1)
                          + " kHz  |  " + (sample->getNumChannels() > 1 ? "Stereo" : "Mono");
        if (status.state == State::Loading) canvas.fileInfo << "  (playing previous)";
    }
    else
    {
        canvas.fileInfo = status.state == State::Missing ? "File not found" : "WAV / AIFF / FLAC / OGG";
    }

    if (! juce::approximatelyEqual (duration, shownDuration))
    {
        shownDuration = duration;
        for (auto* k : { &sampleStart, &sampleEnd, &loopStart, &loopEnd })
        {
            if (duration > 0.0)
                k->getSlider().textFromValueFunction = [duration] (double v) { return juce::String (v * duration, 2) + " s"; };
            else
                k->getSlider().textFromValueFunction = [] (double v) { return juce::String (v * 100.0, 1) + " %"; };
            k->repaint();
        }
    }
    canvas.repaint();
}

void LHSampleSynthEditor::timerCallback()
{
    waveform.refreshIfParametersChanged();
    const int voices = processor.getActiveVoiceCount();
    if (voices != shownVoices)
    {
        shownVoices = voices;
        canvas.footer = "VST3 / AU / CLAP  |  Active voices: " + juce::String (voices) + " / 16  |  Open source (AGPLv3)";
        canvas.repaint (0, 480, 1300, 40);
    }
}
