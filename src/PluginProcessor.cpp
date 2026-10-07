#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "parameters/ParameterIDs.h"
#include "parameters/ParameterLayout.h"

namespace ids = lhss::ids;

LHSampleSynthProcessor::LHSampleSynthProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "LHSampleSynthState", lhss::createParameterLayout())
{
    auto p = [this] (const char* id)
    {
        auto* ptr = apvts.getRawParameterValue (id);
        jassert (ptr != nullptr);
        return ptr;
    };
    raw = { p (ids::rootNote), p (ids::playbackMode), p (ids::sampleStart), p (ids::sampleEnd), p (ids::attack),
            p (ids::decay), p (ids::sustain), p (ids::release), p (ids::triggerMode), p (ids::loopOn),
            p (ids::loopStart), p (ids::loopEnd), p (ids::loopCrossfade), p (ids::grainSize), p (ids::grainDensity),
            p (ids::grainPosRandom), p (ids::pitchRandom), p (ids::stereoSpread), p (ids::freeze), p (ids::reverse),
            p (ids::filterMode), p (ids::cutoff), p (ids::resonance), p (ids::formant), p (ids::pan),
            p (ids::stereoWidth), p (ids::outputGain), p (ids::velocitySens), p (ids::polyphony),
            p (ids::lfoShape), p (ids::lfoRate), p (ids::lfoSync), p (ids::lfoDivision), p (ids::lfoToPitch),
            p (ids::lfoToCutoff), p (ids::lfoToAmp), p (ids::lfoToPan), p (ids::lfoToGrainPos),
            p (ids::fenvAttack), p (ids::fenvDecay), p (ids::fenvSustain), p (ids::fenvRelease),
            p (ids::fenvAmount), p (ids::velToFilter),
            p (ids::voiceMode), p (ids::glide), p (ids::coarseTune), p (ids::fineTune), p (ids::unisonVoices),
            p (ids::unisonDetune), p (ids::drive),
            p (ids::chorusRate), p (ids::chorusDepth), p (ids::chorusMix),
            p (ids::delayTime), p (ids::delaySync), p (ids::delayDivision), p (ids::delayFeedback),
            p (ids::delayMix), p (ids::delayPingPong),
            p (ids::reverbSize), p (ids::reverbDamping), p (ids::reverbMix),
            p (ids::rootTune), p (ids::lfoOn) };

    startTimer (500); // garbage-collect replaced samples on the message thread
}

LHSampleSynthProcessor::~LHSampleSynthProcessor()
{
    stopTimer();
    loaderPool.removeAllJobs (true, 10000);
    cancelPendingUpdate();
}

//==============================================================================
void LHSampleSynthProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate, juce::jmax (samplesPerBlock, 512));
    engine.setParameters (readParameters());
}

void LHSampleSynthProcessor::releaseResources() { engine.reset(); }

bool LHSampleSynthProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

lhss::EngineParams LHSampleSynthProcessor::readParameters() const noexcept
{
    auto f = [] (const std::atomic<float>* a) { return a->load (std::memory_order_relaxed); };
    auto b = [&f] (const std::atomic<float>* a) { return f (a) >= 0.5f; };
    auto i = [&f] (const std::atomic<float>* a) { return juce::roundToInt (f (a)); };

    lhss::EngineParams e;
    e.rootNote = i (raw.rootNote);
    e.rootTuneCents = f (raw.rootTune);
    e.playbackMode = static_cast<lhss::PlaybackMode> (juce::jlimit (0, 1, i (raw.playbackMode)));
    e.sampleStart = f (raw.sampleStart);
    e.sampleEnd = f (raw.sampleEnd);
    e.attackMs = f (raw.attack);
    e.decayMs = f (raw.decay);
    e.sustain = f (raw.sustain);
    e.releaseMs = f (raw.release);
    e.triggerMode = static_cast<lhss::TriggerMode> (juce::jlimit (0, 1, i (raw.triggerMode)));
    e.loopOn = b (raw.loopOn);
    e.loopStart = f (raw.loopStart);
    e.loopEnd = f (raw.loopEnd);
    e.loopCrossfadeMs = f (raw.loopCrossfade);
    e.grainSizeMs = f (raw.grainSize);
    e.grainDensity = f (raw.grainDensity);
    e.grainPosRandom = f (raw.grainPosRandom);
    e.pitchRandom = f (raw.pitchRandom);
    e.stereoSpread = f (raw.stereoSpread);
    e.freeze = b (raw.freeze);
    e.reverse = b (raw.reverse);
    e.filterMode = static_cast<lhss::FilterMode> (juce::jlimit (0, 2, i (raw.filterMode)));
    e.cutoffHz = f (raw.cutoff);
    e.resonance = f (raw.resonance);
    e.formant = f (raw.formant);
    e.pan = f (raw.pan);
    e.stereoWidth = f (raw.stereoWidth);
    e.outputGainDb = f (raw.outputGain);
    e.velocitySensitivity = f (raw.velocitySens);
    e.polyphony = i (raw.polyphony);

    e.lfoOn = b (raw.lfoOn);
    e.lfoShape = static_cast<lhss::LfoShape> (juce::jlimit (0, 4, i (raw.lfoShape)));
    e.lfoRateHz = f (raw.lfoRate);
    e.lfoSync = b (raw.lfoSync);
    e.lfoDivision = i (raw.lfoDivision);
    e.lfoToPitch = f (raw.lfoToPitch);
    e.lfoToCutoff = f (raw.lfoToCutoff);
    e.lfoToAmp = f (raw.lfoToAmp);
    e.lfoToPan = f (raw.lfoToPan);
    e.lfoToGrainPos = f (raw.lfoToGrainPos);
    e.fenvAttackMs = f (raw.fenvAttack);
    e.fenvDecayMs = f (raw.fenvDecay);
    e.fenvSustain = f (raw.fenvSustain);
    e.fenvReleaseMs = f (raw.fenvRelease);
    e.fenvAmount = f (raw.fenvAmount);
    e.velToFilter = f (raw.velToFilter);
    e.voiceMode = static_cast<lhss::VoiceMode> (juce::jlimit (0, 2, i (raw.voiceMode)));
    e.glideMs = f (raw.glide);
    e.coarseTune = i (raw.coarseTune);
    e.fineTune = f (raw.fineTune);
    e.unisonVoices = i (raw.unisonVoices);
    e.unisonDetune = f (raw.unisonDetune);
    e.drive = f (raw.drive);
    e.chorusRate = f (raw.chorusRate);
    e.chorusDepth = f (raw.chorusDepth);
    e.chorusMix = f (raw.chorusMix);
    e.delayTimeMs = f (raw.delayTime);
    e.delaySync = b (raw.delaySync);
    e.delayDivision = i (raw.delayDivision);
    e.delayFeedback = f (raw.delayFeedback);
    e.delayMix = f (raw.delayMix);
    e.delayPingPong = b (raw.delayPingPong);
    e.reverbSize = f (raw.reverbSize);
    e.reverbDamping = f (raw.reverbDamping);
    e.reverbMix = f (raw.reverbMix);
    return e;
}

void LHSampleSynthProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    // Realtime: parameter reads are atomic loads; the engine does no I/O, locking or allocation.
    juce::ScopedNoDenormals noDenormals;
    engine.setParameters (readParameters());
    if (auto* hostPlayHead = getPlayHead())
        if (const auto pos = hostPlayHead->getPosition())
            if (const auto bpm = pos->getBpm()) engine.setTempo (*bpm);
    keyboardState.processNextMidiBuffer (midi, 0, buffer.getNumSamples(), true);
    engine.process (buffer, midi);
    activeVoices.store (engine.getActiveVoiceCount(), std::memory_order_relaxed);
}

juce::AudioProcessorEditor* LHSampleSynthProcessor::createEditor() { return new LHSampleSynthEditor (*this); }

//==============================================================================
// Sample loading: decoding runs on the loader thread (or the caller for loadSampleSync);
// the result is published through SampleStore, which the audio thread picks up lock-free.
void LHSampleSynthProcessor::setStatus (SampleState s, const juce::String& path, const juce::String& message)
{
    {
        const std::scoped_lock lock (statusMutex);
        status = { s, path, message };
    }
    sampleBroadcaster.sendChangeMessage();
}

LHSampleSynthProcessor::SampleStatus LHSampleSynthProcessor::getSampleStatus() const
{
    const std::scoped_lock lock (statusMutex);
    return status;
}

void LHSampleSynthProcessor::runLoad (const juce::File& file, int requestId, bool tune)
{
    auto result = lhss::SampleLoader::loadFile (file);   // also analyses the pitch
    if (requestId != latestRequest.load()) return; // a newer request superseded this one

    if (result.ok())
    {
        sampleStore.publish (result.sample, engine);
        // Parameters are changed on the message thread (host gestures, GUI attachments).
        if (! tune) pendingTune = false;
        else if (juce::MessageManager::getInstanceWithoutCreating() != nullptr
                 && juce::MessageManager::getInstance()->isThisTheMessageThread())
            applyPitch (result.sample->getPitch());
        else
        {
            pendingTune = true;
            triggerAsyncUpdate();
        }
        setStatus (SampleState::Loaded, file.getFullPathName());
    }
    else
    {
        setStatus (file.existsAsFile() ? SampleState::Error : SampleState::Missing, file.getFullPathName(), result.error);
    }
}

void LHSampleSynthProcessor::loadSampleAsync (const juce::File& file, bool tune)
{
    const int id = ++latestRequest;
    setStatus (SampleState::Loading, file.getFullPathName());
    loaderPool.addJob ([this, file, id, tune] { runLoad (file, id, tune); });
}

bool LHSampleSynthProcessor::loadSampleSync (const juce::File& file, bool tune)
{
    const int id = ++latestRequest;
    runLoad (file, id, tune);
    return getSampleStatus().state == SampleState::Loaded;
}

void LHSampleSynthProcessor::handleAsyncUpdate()
{
    if (pendingTune.exchange (false)) tuneToDetectedPitch();
}

bool LHSampleSynthProcessor::tuneToDetectedPitch()
{
    const auto sample = sampleStore.latest();
    if (sample == nullptr) return false;
    applyPitch (sample->getPitch());
    return sample->getPitch().valid;
}

void LHSampleSynthProcessor::setParameterWithGesture (const char* id, float value)
{
    if (auto* param = apvts.getParameter (id))
    {
        param->beginChangeGesture();
        param->setValueNotifyingHost (param->convertTo0to1 (value));
        param->endChangeGesture();
    }
}

void LHSampleSynthProcessor::applyPitch (const lhss::PitchInfo& pitch)
{
    // A sample with no clear pitch (noise, clicks) is played as recorded on C4 (MIDI 60).
    setParameterWithGesture (ids::rootNote, pitch.valid ? (float) pitch.nearestNote() : 60.0f);
    setParameterWithGesture (ids::rootTune, pitch.valid ? pitch.centsOffset() : 0.0f);
}

bool LHSampleSynthProcessor::waitForPendingLoads (int timeoutMs)
{
    const auto deadline = juce::Time::getMillisecondCounter() + (juce::uint32) timeoutMs;
    while (loaderPool.getNumJobs() > 0)
    {
        if (juce::Time::getMillisecondCounter() > deadline) return false;
        juce::Thread::sleep (5);
    }
    return true;
}

void LHSampleSynthProcessor::resetParametersToDefaults()
{
    for (auto* p : getParameters())
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (p))
        {
            ranged->beginChangeGesture();
            ranged->setValueNotifyingHost (ranged->getDefaultValue());
            ranged->endChangeGesture();
        }
    // The tuning belongs to the (kept) sample, so put it back.
    if (sampleStore.latest() != nullptr) tuneToDetectedPitch();
}

void LHSampleSynthProcessor::timerCallback() { sampleStore.collectGarbage (engine); }

//==============================================================================
// State: all APVTS parameters (which include playback mode, loop settings and sample region)
// plus the sample's file path. The audio itself is never embedded — the file is re-decoded
// on restore. If it no longer exists, parameters are still restored, the path is kept (so a
// re-save does not lose it) and the GUI shows the sample as missing.
void LHSampleSynthProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty (kSamplePathProperty, getSampleStatus().path, nullptr);
    state.setProperty ("pluginVersion", LHSS_VERSION_STRING, nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, destData);
}

void LHSampleSynthProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    const auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType())) return;

    auto tree = juce::ValueTree::fromXml (*xml);
    const juce::String path = tree.getProperty (kSamplePathProperty).toString();
    tree.removeProperty (kSamplePathProperty, nullptr);
    tree.removeProperty ("pluginVersion", nullptr);

    // Projects saved before the LFO switch existed: keep their LFO running if they use it.
    if (! tree.getChildWithProperty ("id", ids::lfoOn).isValid())
    {
        bool usesLfo = false;
        for (auto* id : { ids::lfoToPitch, ids::lfoToCutoff, ids::lfoToAmp, ids::lfoToPan, ids::lfoToGrainPos })
            if (std::abs (static_cast<float> (tree.getChildWithProperty ("id", id).getProperty ("value", 0.0f))) > 1.0e-6f)
                usesLfo = true;
        juce::ValueTree lfoParam ("PARAM");
        lfoParam.setProperty ("id", ids::lfoOn, nullptr);
        lfoParam.setProperty ("value", usesLfo ? 1.0f : 0.0f, nullptr);
        tree.appendChild (lfoParam, nullptr);
    }
    apvts.replaceState (tree);
    pendingTune = false; // a load still waiting to re-tune must not override the restored tuning

    // Ask hosts to re-read every parameter value (CLAP: params rescan; VST3/AU: refresh).
    updateHostDisplay (ChangeDetails().withParameterInfoChanged (true));

    if (path.isEmpty()) return;

    const juce::File file (path);
    if (file.existsAsFile())
    {
        const auto current = sampleStore.latest();
        // Keep the project's saved Root Note / Root Tune (no re-tuning on restore).
        if (current == nullptr || current->getPath() != path) loadSampleAsync (file, false);
        else setStatus (SampleState::Loaded, path);
    }
    else
    {
        ++latestRequest; // cancel any in-flight load
        setStatus (SampleState::Missing, path, "Sample file not found: " + path);
    }
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new LHSampleSynthProcessor(); }
