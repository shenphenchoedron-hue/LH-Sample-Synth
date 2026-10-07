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
    int x, y, w;
    juce::Colour colour;
    const char* title;
};

constexpr int kRow1 = 112, kRow2 = 478, kPanelHeight = 352;

const Panel panels[] = {
    { 38,   kRow1, 520, col::green,    "Sample" },
    { 570,  kRow1, 292, col::orange,   "Loop" },
    { 874,  kRow1, 420, col::pink,     "Envelope (ADSR)" },
    { 1306, kRow1, 604, col::purple,   "Granular" },
    { 1922, kRow1, 230, col::cyan,     "Filter" },
    { 2164, kRow1, 198, col::outGreen, "Output" },
    { 38,   kRow2, 560, col::blue,     "LFO" },
    { 610,  kRow2, 420, col::cyan,     "Filter Envelope" },
    { 1042, kRow2, 420, col::orange,   "Voice" },
    { 1474, kRow2, 150, col::coral,    "Drive" },
    { 1636, kRow2, 726, col::purple,   "Effects" },
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

    const auto frame = juce::Rectangle<float> (18, 20, 2364, 870);
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
        const auto r = juce::Rectangle<float> ((float) p.x, (float) p.y, (float) p.w, (float) kPanelHeight);
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
        g.fillRoundedRectangle ((float) p.x + 12.0f, (float) p.y + 10.0f, 6.0f, 19.0f, 3.0f);
        g.setFont (uiFont (15.0f));
        g.setColour (col::section);
        g.drawText (p.title, p.x + 28, p.y + 16, p.w - 40, 16, juce::Justification::left, false);
    }

    drawLabel (g, "Loop On", 594, 166);
    drawLabel (g, "Mode", 1952, 177);

    // Second row labels
    drawLabel (g, "Shape", 58, 538);
    drawLabel (g, "Sync", 236, 538);
    drawLabel (g, "Division", 316, 538);
    drawLabel (g, "Voice Mode", 1062, 538);

    auto subHeading = [&g] (const char* text, int x, int y, juce::Colour c)
    {
        g.setFont (uiFont (12.0f));
        g.setColour (c);
        g.drawText (text, x, y - 12, 200, 15, juce::Justification::left, false);
    };
    subHeading ("CHORUS", 1656, 540, col::purple);
    subHeading ("DELAY", 1886, 540, col::purple);
    subHeading ("REVERB", 2176, 540, col::purple);
    g.setColour (col::purple.withAlpha (0.2f));
    g.fillRect (1868, 530, 1, 290);
    g.fillRect (2158, 530, 1, 290);
    drawLabel (g, "Sync", 1890, 703);
    drawLabel (g, "Division", 1964, 703);
    drawLabel (g, "Ping Pong", 2080, 703);

    g.setFont (uiFont (10.0f, false));
    g.setColour (col::tiny);
    g.drawText (footer, 48, 860, 1200, 14, juce::Justification::left, false);
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
      outputGain (p.getAPVTS(), ids::outputGain, "Output Gain", col::outGreen),
      voiceMode (*p.getAPVTS().getParameter (ids::voiceMode), { "Poly", "Mono", "Legato" }, col::orange, { 78, 78, 78 }, 6),
      lfoRate (p.getAPVTS(), ids::lfoRate, "Rate", col::blue),
      lfoToPitch (p.getAPVTS(), ids::lfoToPitch, "> Pitch", col::blue),
      lfoToCutoff (p.getAPVTS(), ids::lfoToCutoff, "> Cutoff", col::blue),
      lfoToAmp (p.getAPVTS(), ids::lfoToAmp, "> Amp", col::blue),
      lfoToPan (p.getAPVTS(), ids::lfoToPan, "> Pan", col::blue),
      lfoToGrainPos (p.getAPVTS(), ids::lfoToGrainPos, "> Grain Pos", col::blue),
      fenvAttack (p.getAPVTS(), ids::fenvAttack, "Attack", col::cyan),
      fenvDecay (p.getAPVTS(), ids::fenvDecay, "Decay", col::cyan),
      fenvSustain (p.getAPVTS(), ids::fenvSustain, "Sustain", col::cyan),
      fenvRelease (p.getAPVTS(), ids::fenvRelease, "Release", col::cyan),
      fenvAmount (p.getAPVTS(), ids::fenvAmount, "Env Amount", col::cyan),
      velToFilter (p.getAPVTS(), ids::velToFilter, "Vel > Filter", col::coral),
      glide (p.getAPVTS(), ids::glide, "Glide", col::orange),
      coarseTune (p.getAPVTS(), ids::coarseTune, "Coarse", col::orange),
      fineTune (p.getAPVTS(), ids::fineTune, "Fine", col::orange),
      unisonVoices (p.getAPVTS(), ids::unisonVoices, "Unison", col::orange),
      unisonDetune (p.getAPVTS(), ids::unisonDetune, "Detune", col::orange),
      drive (p.getAPVTS(), ids::drive, "Drive", col::coral),
      chorusRate (p.getAPVTS(), ids::chorusRate, "Rate", col::purple),
      chorusDepth (p.getAPVTS(), ids::chorusDepth, "Depth", col::purple),
      chorusMix (p.getAPVTS(), ids::chorusMix, "Mix", col::purple),
      delayTime (p.getAPVTS(), ids::delayTime, "Time", col::purple),
      delayFeedback (p.getAPVTS(), ids::delayFeedback, "Feedback", col::purple),
      delayMix (p.getAPVTS(), ids::delayMix, "Mix", col::purple),
      reverbSize (p.getAPVTS(), ids::reverbSize, "Size", col::purple),
      reverbDamping (p.getAPVTS(), ids::reverbDamping, "Damping", col::purple),
      reverbMix (p.getAPVTS(), ids::reverbMix, "Mix", col::purple)
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

    for (auto* k : { &lfoRate, &lfoToPitch, &lfoToCutoff, &lfoToAmp, &lfoToPan, &lfoToGrainPos, &fenvAttack,
                     &fenvDecay, &fenvSustain, &fenvRelease, &fenvAmount, &velToFilter, &glide, &coarseTune,
                     &fineTune, &unisonVoices, &unisonDetune, &drive, &chorusRate, &chorusDepth, &chorusMix,
                     &delayTime, &delayFeedback, &delayMix, &reverbSize, &reverbDamping, &reverbMix })
        canvas.addAndMakeVisible (k);
    canvas.addAndMakeVisible (voiceMode);

    auto setupCombo = [this] (juce::ComboBox& box, const juce::StringArray& items, const char* id,
                              std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>& att)
    {
        box.addItemList (items, 1);
        canvas.addAndMakeVisible (box);
        att = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (processor.getAPVTS(), id, box);
    };
    juce::StringArray divisions;
    for (auto* d : lhss::kSyncDivisionNames) divisions.add (d);
    setupCombo (lfoShapeBox, { "Sine", "Triangle", "Square", "Saw", "Random" }, ids::lfoShape, lfoShapeAttach);
    setupCombo (lfoDivisionBox, divisions, ids::lfoDivision, lfoDivisionAttach);
    setupCombo (delayDivisionBox, divisions, ids::delayDivision, delayDivisionAttach);
    setupSwitch (lfoSyncSwitch, col::blue, ids::lfoSync, lfoSyncAttach);
    setupSwitch (delaySyncSwitch, col::purple, ids::delaySync, delaySyncAttach);
    setupSwitch (pingPongSwitch, col::purple, ids::delayPingPong, pingPongAttach);

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

    // ---- Second row ------------------------------------------------------------------------
    lfoShapeBox.setBounds (58, 546, 160, 32);
    lfoSyncSwitch.setBounds (236, 549, 54, 26);
    lfoDivisionBox.setBounds (316, 546, 110, 32);
    const int lfoX[] = { 88, 180, 272, 364, 456, 548 };
    juce::Component* lfoKnobs[] = { &lfoRate, &lfoToPitch, &lfoToCutoff, &lfoToAmp, &lfoToPan, &lfoToGrainPos };
    for (int i = 0; i < 6; ++i) placeKnob (*lfoKnobs[i], lfoX[i], 640);

    placeKnob (fenvAttack, 670, 540);
    placeKnob (fenvDecay, 762, 540);
    placeKnob (fenvSustain, 854, 540);
    placeKnob (fenvRelease, 946, 540);
    placeKnob (fenvAmount, 716, 690);
    placeKnob (velToFilter, 900, 690);

    voiceMode.setBounds (1062, 546, 246, 32);
    const int voiceX[] = { 1090, 1174, 1258, 1342, 1426 };
    juce::Component* voiceKnobs[] = { &glide, &coarseTune, &fineTune, &unisonVoices, &unisonDetune };
    for (int i = 0; i < 5; ++i) placeKnob (*voiceKnobs[i], voiceX[i], 640, 84, 116);

    placeKnob (drive, 1549, 640);

    placeKnob (chorusRate, 1706, 562);
    placeKnob (chorusDepth, 1796, 562);
    placeKnob (chorusMix, 1751, 690);
    placeKnob (delayTime, 1918, 562);
    placeKnob (delayFeedback, 2012, 562);
    placeKnob (delayMix, 2106, 562);
    delaySyncSwitch.setBounds (1890, 712, 54, 26);
    delayDivisionBox.setBounds (1964, 709, 100, 32);
    pingPongSwitch.setBounds (2080, 712, 54, 26);
    placeKnob (reverbSize, 2214, 562);
    placeKnob (reverbDamping, 2306, 562);
    placeKnob (reverbMix, 2260, 690);
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
    // Rate knobs are inactive while their tempo sync is on (and vice versa).
    const bool lfoSynced = lfoSyncSwitch.getToggleState(), delaySynced = delaySyncSwitch.getToggleState();
    lfoRate.setAlpha (lfoSynced ? 0.35f : 1.0f);
    lfoDivisionBox.setAlpha (lfoSynced ? 1.0f : 0.45f);
    delayTime.setAlpha (delaySynced ? 0.35f : 1.0f);
    delayDivisionBox.setAlpha (delaySynced ? 1.0f : 0.45f);

    waveform.refreshIfParametersChanged();
    const int voices = processor.getActiveVoiceCount();
    if (voices != shownVoices)
    {
        shownVoices = voices;
        canvas.footer = "VST3 / AU / CLAP  |  Active voices: " + juce::String (voices) + " / 16  |  Open source (AGPLv3)";
        canvas.repaint (0, 850, 1300, 40);
    }
}
