#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>
#include <mutex>

#include "engine/InstrumentEngine.h"
#include "engine/SampleLoader.h"
#include "engine/SampleStore.h"

/** Plugin wrapper shared by VST3, AU, CLAP (via clap-juce-extensions) and Standalone.
    It only translates host concepts (APVTS parameters, state, buses) into calls on the
    format-independent lhss::InstrumentEngine. */
class LHSampleSynthProcessor final : public juce::AudioProcessor,
                                     private juce::Timer
{
public:
    enum class SampleState { None, Loading, Loaded, Missing, Error };

    struct SampleStatus
    {
        SampleState state = SampleState::None;
        juce::String path;     // file path stored in the plugin state
        juce::String message;  // error text for the GUI
    };

    LHSampleSynthProcessor();
    ~LHSampleSynthProcessor() override;

    // AudioProcessor
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "LH Sample Synth"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 4.0; } // delay / reverb tails

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // Sample management (message / loader threads only — never the audio thread)
    void loadSampleAsync (const juce::File& file);
    bool loadSampleSync (const juce::File& file);   // blocking variant (tests, offline tools)
    bool waitForPendingLoads (int timeoutMs);
    std::shared_ptr<const lhss::SampleData> getLoadedSample() const { return sampleStore.latest(); }
    SampleStatus getSampleStatus() const;

    /** Sets every parameter back to its default (with host gestures). The loaded sample is kept. */
    void resetParametersToDefaults();

    juce::AudioProcessorValueTreeState& getAPVTS() noexcept { return apvts; }
    juce::ChangeBroadcaster& getSampleBroadcaster() noexcept { return sampleBroadcaster; }
    lhss::EngineParams readParameters() const noexcept;
    int getActiveVoiceCount() const noexcept { return activeVoices.load (std::memory_order_relaxed); }

    static constexpr const char* kSamplePathProperty = "samplePath";

private:
    void timerCallback() override;
    void runLoad (const juce::File& file, int requestId);
    void setStatus (SampleState s, const juce::String& path, const juce::String& message = {});

    juce::AudioProcessorValueTreeState apvts;

    // Declaration order matters: the engine (raw pointers) is destroyed before the store
    // (owning pointers), and the loader pool (which uses both) is destroyed first of all.
    lhss::SampleStore sampleStore;
    lhss::InstrumentEngine engine;

    struct RawParams
    {
        std::atomic<float>* rootNote; std::atomic<float>* playbackMode; std::atomic<float>* sampleStart;
        std::atomic<float>* sampleEnd; std::atomic<float>* attack; std::atomic<float>* decay;
        std::atomic<float>* sustain; std::atomic<float>* release; std::atomic<float>* triggerMode;
        std::atomic<float>* loopOn; std::atomic<float>* loopStart; std::atomic<float>* loopEnd;
        std::atomic<float>* loopCrossfade; std::atomic<float>* grainSize; std::atomic<float>* grainDensity;
        std::atomic<float>* grainPosRandom; std::atomic<float>* pitchRandom; std::atomic<float>* stereoSpread;
        std::atomic<float>* freeze; std::atomic<float>* reverse; std::atomic<float>* filterMode;
        std::atomic<float>* cutoff; std::atomic<float>* resonance; std::atomic<float>* formant;
        std::atomic<float>* pan; std::atomic<float>* stereoWidth; std::atomic<float>* outputGain;
        std::atomic<float>* velocitySens; std::atomic<float>* polyphony;
        // Synth section
        std::atomic<float>* lfoShape; std::atomic<float>* lfoRate; std::atomic<float>* lfoSync;
        std::atomic<float>* lfoDivision; std::atomic<float>* lfoToPitch; std::atomic<float>* lfoToCutoff;
        std::atomic<float>* lfoToAmp; std::atomic<float>* lfoToPan; std::atomic<float>* lfoToGrainPos;
        std::atomic<float>* fenvAttack; std::atomic<float>* fenvDecay; std::atomic<float>* fenvSustain;
        std::atomic<float>* fenvRelease; std::atomic<float>* fenvAmount; std::atomic<float>* velToFilter;
        std::atomic<float>* voiceMode; std::atomic<float>* glide; std::atomic<float>* coarseTune;
        std::atomic<float>* fineTune; std::atomic<float>* unisonVoices; std::atomic<float>* unisonDetune;
        std::atomic<float>* drive;
        std::atomic<float>* chorusRate; std::atomic<float>* chorusDepth; std::atomic<float>* chorusMix;
        std::atomic<float>* delayTime; std::atomic<float>* delaySync; std::atomic<float>* delayDivision;
        std::atomic<float>* delayFeedback; std::atomic<float>* delayMix; std::atomic<float>* delayPingPong;
        std::atomic<float>* reverbSize; std::atomic<float>* reverbDamping; std::atomic<float>* reverbMix;
    } raw {};

    mutable std::mutex statusMutex;
    SampleStatus status;
    juce::ChangeBroadcaster sampleBroadcaster;
    std::atomic<int> latestRequest { 0 };
    std::atomic<int> activeVoices { 0 };

    juce::ThreadPool loaderPool { juce::ThreadPoolOptions{}.withThreadName ("LHSS Loader").withNumberOfThreads (1) };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LHSampleSynthProcessor)
};
