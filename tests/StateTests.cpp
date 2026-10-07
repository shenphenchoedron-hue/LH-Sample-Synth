#include <juce_audio_formats/juce_audio_formats.h>

#include "PluginProcessor.h"
#include "TestHelpers.h"
#include "parameters/ParameterIDs.h"

using namespace lhss;

namespace
{
juce::File writeTestWav (const juce::File& file, double sr, int channels, double seconds, double freq = 440.0)
{
    file.deleteFile();
    auto buffer = test::makeSine (freq, seconds, sr, channels);
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::OutputStream> stream (file.createOutputStream().release());
    auto writer = wav.createWriterFor (stream, juce::AudioFormatWriterOptions{}
                                                   .withSampleRate (sr)
                                                   .withNumChannels (channels)
                                                   .withBitsPerSample (24));
    if (writer != nullptr)
        writer->writeFromAudioSampleBuffer (buffer, 0, buffer.getNumSamples());
    return file;
}

void setParam (LHSampleSynthProcessor& p, const char* id, float value)
{
    auto* param = p.getAPVTS().getParameter (id);
    param->setValueNotifyingHost (param->convertTo0to1 (value));
}

float getParam (LHSampleSynthProcessor& p, const char* id)
{
    return p.getAPVTS().getRawParameterValue (id)->load();
}

/** Plays one held note through the full processor and returns the output frequency, measured
    independently of PitchDetector from interpolated rising zero crossings (0.1 s .. 1.0 s). */
double playedFrequency (LHSampleSynthProcessor& p, int note, double hostRate)
{
    p.setPlayConfigDetails (0, 2, hostRate, 512);
    p.prepareToPlay (hostRate, 512);
    std::vector<float> out;
    juce::AudioBuffer<float> buf (2, 512);
    for (int b = 0; b < static_cast<int> (hostRate * 1.1 / 512); ++b)
    {
        juce::MidiBuffer midi;
        if (b == 0) midi.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) 100), 0);
        buf.clear();
        p.processBlock (buf, midi);
        for (int i = 0; i < 512; ++i) out.push_back (buf.getSample (0, i));
    }
    double first = -1.0, last = -1.0;
    int periods = 0;
    for (size_t i = static_cast<size_t> (hostRate * 0.1); i < static_cast<size_t> (hostRate * 1.0) && i < out.size(); ++i)
        if (out[i - 1] < 0.0f && out[i] >= 0.0f)
        {
            const double t = static_cast<double> (i - 1) + out[i - 1] / (out[i - 1] - out[i]);
            if (first < 0.0) first = t; else ++periods;
            last = t;
        }
    return periods > 0 ? periods * hostRate / (last - first) : 0.0;
}
} // namespace

class StateTests final : public juce::UnitTest
{
public:
    StateTests() : juce::UnitTest ("State save / restore", "LHSampleSynth") {}

    void runTest() override
    {
        const auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("lhss_tests");
        dir.createDirectory();
        const auto wavFile = writeTestWav (dir.getChildFile ("glass.wav"), 48000.0, 2, 0.75);

        beginTest ("Parameter clamping through the APVTS ranges");
        {
            LHSampleSynthProcessor p;
            auto* root = p.getAPVTS().getParameter (ids::rootNote);
            root->setValueNotifyingHost (1.7f);
            expectEquals (getParam (p, ids::rootNote), 127.0f);
            root->setValueNotifyingHost (-1.0f);
            expectEquals (getParam (p, ids::rootNote), 0.0f);
            setParam (p, ids::cutoff, 1.0e6f);
            expectEquals (getParam (p, ids::cutoff), 20000.0f);
            setParam (p, ids::outputGain, -500.0f);
            expectEquals (getParam (p, ids::outputGain), -48.0f);
            expectEquals (p.getAPVTS().getParameter (ids::rootNote)->getCurrentValueAsText(), juce::String ("C-1"));
            setParam (p, ids::rootNote, 60.0f);
            expectEquals (p.getAPVTS().getParameter (ids::rootNote)->getCurrentValueAsText(), juce::String ("C4"));
        }

        beginTest ("State serialization");
        juce::MemoryBlock saved;
        {
            LHSampleSynthProcessor p;
            expect (p.loadSampleSync (wavFile), "test wav must load");
            setParam (p, ids::rootNote, 57.0f);
            setParam (p, ids::playbackMode, 0.0f);
            setParam (p, ids::sampleStart, 0.1f);
            setParam (p, ids::sampleEnd, 0.9f);
            setParam (p, ids::loopOn, 1.0f);
            setParam (p, ids::loopStart, 0.3f);
            setParam (p, ids::loopEnd, 0.7f);
            setParam (p, ids::loopCrossfade, 120.0f);
            setParam (p, ids::triggerMode, 1.0f);
            setParam (p, ids::reverse, 1.0f);
            setParam (p, ids::grainSize, 150.0f);
            setParam (p, ids::formant, -0.5f);
            setParam (p, ids::cutoff, 2800.0f);
            setParam (p, ids::filterMode, 2.0f);
            p.getStateInformation (saved);
            expect (saved.getSize() > 0);
            expect (saved.getSize() < 64 * 1024, "audio must not be embedded in the state");
            const auto xml = juce::AudioProcessor::getXmlFromBinary (saved.getData(), (int) saved.getSize());
            expect (xml != nullptr && xml->getStringAttribute (LHSampleSynthProcessor::kSamplePathProperty) == wavFile.getFullPathName());
        }

        beginTest ("State restoration");
        {
            LHSampleSynthProcessor p;
            p.setStateInformation (saved.getData(), (int) saved.getSize());
            expectEquals (getParam (p, ids::rootNote), 57.0f);
            expectEquals (getParam (p, ids::playbackMode), 0.0f);
            expectWithinAbsoluteError (getParam (p, ids::sampleStart), 0.1f, 1e-4f);
            expectWithinAbsoluteError (getParam (p, ids::sampleEnd), 0.9f, 1e-4f);
            expectEquals (getParam (p, ids::loopOn), 1.0f);
            expectWithinAbsoluteError (getParam (p, ids::loopStart), 0.3f, 1e-4f);
            expectWithinAbsoluteError (getParam (p, ids::loopEnd), 0.7f, 1e-4f);
            expectWithinAbsoluteError (getParam (p, ids::loopCrossfade), 120.0f, 0.01f);
            expectEquals (getParam (p, ids::triggerMode), 1.0f);
            expectEquals (getParam (p, ids::reverse), 1.0f);
            expectWithinAbsoluteError (getParam (p, ids::grainSize), 150.0f, 0.01f);
            expectWithinAbsoluteError (getParam (p, ids::formant), -0.5f, 1e-4f);
            expectWithinAbsoluteError (getParam (p, ids::cutoff), 2800.0f, 0.1f);
            expectEquals (getParam (p, ids::filterMode), 2.0f);

            expect (p.waitForPendingLoads (10000));
            const auto status = p.getSampleStatus();
            expect (status.state == LHSampleSynthProcessor::SampleState::Loaded);
            expectEquals (status.path, wavFile.getFullPathName());
            const auto s = p.getLoadedSample();
            expect (s != nullptr);
            if (s != nullptr)
            {
                expectEquals (s->getNumChannels(), 2);
                expectEquals (s->getNumFrames(), 36000);
                expectEquals (s->getSampleRate(), 48000.0);
            }

            // Restored instance actually plays.
            p.prepareToPlay (44100.0, 512);
            juce::AudioBuffer<float> buf (2, 512);
            juce::MidiBuffer midi;
            midi.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
            p.processBlock (buf, midi);
            midi.clear();
            p.processBlock (buf, midi);
            expect (buf.getMagnitude (0, 512) > 0.0f);
        }

        beginTest ("Missing sample file: no crash, parameters kept, shown as missing");
        {
            const auto missing = dir.getChildFile ("moved_away.wav");
            {
                juce::MemoryBlock withMissing;
                const auto xml = juce::AudioProcessor::getXmlFromBinary (saved.getData(), (int) saved.getSize());
                xml->setAttribute (LHSampleSynthProcessor::kSamplePathProperty, missing.getFullPathName());
                juce::AudioProcessor::copyXmlToBinary (*xml, withMissing);

                LHSampleSynthProcessor p;
                p.setStateInformation (withMissing.getData(), (int) withMissing.getSize());
                expect (p.getSampleStatus().state == LHSampleSynthProcessor::SampleState::Missing);
                expectEquals (p.getSampleStatus().path, missing.getFullPathName());
                expectEquals (getParam (p, ids::rootNote), 57.0f);
                expectWithinAbsoluteError (getParam (p, ids::loopStart), 0.3f, 1e-4f);

                p.prepareToPlay (48000.0, 256);
                juce::AudioBuffer<float> buf (2, 256);
                juce::MidiBuffer midi;
                midi.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
                p.processBlock (buf, midi);
                expectEquals (buf.getMagnitude (0, 256), 0.0f);

                // Re-saving keeps the path so the project can recover once the file is back.
                juce::MemoryBlock resaved;
                p.getStateInformation (resaved);
                const auto xml2 = juce::AudioProcessor::getXmlFromBinary (resaved.getData(), (int) resaved.getSize());
                expect (xml2->getStringAttribute (LHSampleSynthProcessor::kSamplePathProperty) == missing.getFullPathName());
            }
        }

        beginTest ("Host-style parameter writes (setValue + listeners) round-trip through state");
        {
            juce::MemoryBlock block;
            juce::Array<float> written;
            {
                LHSampleSynthProcessor p;
                juce::Random rng (1234);
                for (auto* param : p.getParameters())
                {
                    float v = rng.nextFloat();
                    if (param->isDiscrete() && param->getNumSteps() > 1)
                        v = (float) juce::roundToInt (v * (float) (param->getNumSteps() - 1)) / (float) (param->getNumSteps() - 1);
                    param->setValue (v);
                    param->sendValueChangedMessageToListeners (v);
                    written.add (param->getValue());
                }
                p.getStateInformation (block);
            }
            LHSampleSynthProcessor q;
            q.setStateInformation (block.getData(), (int) block.getSize());
            int mismatches = 0;
            for (int i = 0; i < q.getParameters().size(); ++i)
                if (std::abs (q.getParameters()[i]->getValue() - written[i]) > 1.0e-4f)
                {
                    ++mismatches;
                    logMessage ("mismatch " + q.getParameters()[i]->getName (32) + ": " + juce::String (written[i]) + " vs " + juce::String (q.getParameters()[i]->getValue()));
                }
            expectEquals (mismatches, 0);
        }

        beginTest ("Parameter text round-trips (value -> text -> value -> text)");
        {
            LHSampleSynthProcessor p;
            for (auto* param : p.getParameters())
                for (float v : { 0.0f, 0.13f, 0.5f, 0.77f, 1.0f })
                {
                    const auto t1 = param->getText (v, 64);
                    const auto t2 = param->getText (param->getValueForText (t1), 64);
                    expectEquals (t2, t1, param->getName (32));
                }
        }

        beginTest ("Reset to default restores every parameter and keeps the sample");
        {
            LHSampleSynthProcessor p;
            expect (p.loadSampleSync (wavFile));
            for (auto* param : p.getParameters()) param->setValueNotifyingHost (0.83f);
            p.resetParametersToDefaults();
            int wrong = 0;
            for (auto* param : p.getParameters())
            {
                const auto id = dynamic_cast<juce::AudioProcessorParameterWithID*> (param)->paramID;
                if (id == ids::rootNote || id == ids::rootTune) continue; // re-tuned to the sample, see below
                if (std::abs (param->getValue() - param->getDefaultValue()) > 1.0e-6f) ++wrong;
            }
            expectEquals (wrong, 0);
            expectEquals (getParam (p, ids::rootNote), 69.0f, "the kept 440 Hz sample stays tuned to A4");
            expectWithinAbsoluteError (getParam (p, ids::rootTune), 0.0f, 2.0f);
            expect (p.getLoadedSample() != nullptr);
            expect (p.getSampleStatus().state == LHSampleSynthProcessor::SampleState::Loaded);
        }

        beginTest ("Loading a sample tunes Root Note and Root Tune to its detected pitch");
        {
            // 450 Hz = A4 (MIDI 69) + 38.9 cents; 196 Hz = G3 (MIDI 55) - 0.6 cents.
            for (auto [freq, note, cents] : { std::tuple { 450.0, 69.0f, 38.9f }, std::tuple { 196.0, 55.0f, 0.0f } })
            {
                const auto wav = writeTestWav (dir.getChildFile ("tone.wav"), 44100.0, 1, 1.0, freq);
                LHSampleSynthProcessor p;
                setParam (p, ids::rootNote, 40.0f);
                expect (p.loadSampleSync (wav));
                expectEquals (getParam (p, ids::rootNote), note, juce::String (freq) + " Hz");
                expectWithinAbsoluteError (getParam (p, ids::rootTune), cents, 2.0f, juce::String (freq) + " Hz");
            }
        }

        beginTest ("Control: a clean 440 Hz sample plays 440 Hz on its root key and 261.63 Hz on C4");
        {
            // Neutral settings (defaults: no LFO, tune, random pitch, unison or glide), the file at
            // 44.1 kHz played at 48 and 44.1 kHz host rates, in both playback modes.
            const auto wav = writeTestWav (dir.getChildFile ("a440.wav"), 44100.0, 1, 2.0, 440.0);
            for (double hostRate : { 48000.0, 44100.0 })
                for (float mode : { 0.0f, 1.0f })
                {
                    LHSampleSynthProcessor p;
                    expect (p.loadSampleSync (wav));
                    setParam (p, ids::playbackMode, mode);
                    expectEquals (getParam (p, ids::rootNote), 69.0f, "detected root A4");
                    expectWithinAbsoluteError (getParam (p, ids::rootTune), 0.0f, 0.5f, "no cent offset for a clean 440 Hz");
                    const auto label = juce::String (mode > 0.5f ? "Pitch" : "Natural") + " @ " + juce::String (hostRate);
                    expectWithinAbsoluteError (playedFrequency (p, 69, hostRate), 440.0, 0.3, "A4 " + label);
                    expectWithinAbsoluteError (playedFrequency (p, 60, hostRate), 261.63, 0.3, "C4 " + label);
                }
        }

        beginTest ("A slow LFO > Pitch (as found in the app settings) detunes a clean sample");
        {
            const auto wav = dir.getChildFile ("a440.wav");
            LHSampleSynthProcessor p;
            expect (p.loadSampleSync (wav));
            setParam (p, ids::playbackMode, 0.0f);
            setParam (p, ids::lfoToPitch, 0.43f);   // the value found in the user's app settings
            expectWithinAbsoluteError (playedFrequency (p, 60, 48000.0), 261.63, 0.3, "LFO switch off (default): no detune");
            setParam (p, ids::lfoOn, 1.0f);
            setParam (p, ids::lfoRate, 0.14f);
            const double hz = playedFrequency (p, 60, 48000.0);
            expect (hz > 262.0, "slow LFO > Pitch raises C4 to " + juce::String (hz, 2) + " Hz during the first second");
        }

        beginTest ("Asynchronous loads tune on the message thread; restoring a project keeps its tuning");
        {
            const auto wav = writeTestWav (dir.getChildFile ("tone_async.wav"), 48000.0, 1, 1.0, 330.0); // E4 = 64, -2 ct
            juce::MemoryBlock block;
            {
                LHSampleSynthProcessor p;
                p.loadSampleAsync (wav);
                expect (p.waitForPendingLoads (10000));
                // The re-tune is posted to the message thread (this test runs on it): deliver it.
                for (int i = 0; i < 100 && ! juce::approximatelyEqual (getParam (p, ids::rootNote), 64.0f); ++i)
                {
                    juce::MessageManager::getInstance()->runDispatchLoopUntil (10);
                }
                expectEquals (getParam (p, ids::rootNote), 64.0f);
                setParam (p, ids::rootNote, 52.0f);   // the user overrides it
                setParam (p, ids::rootTune, 10.0f);
                p.getStateInformation (block);
            }
            LHSampleSynthProcessor q;
            q.setStateInformation (block.getData(), (int) block.getSize());
            expect (q.waitForPendingLoads (10000));
            juce::MessageManager::getInstance()->runDispatchLoopUntil (50);
            expectEquals (getParam (q, ids::rootNote), 52.0f);
            expectWithinAbsoluteError (getParam (q, ids::rootTune), 10.0f, 0.01f);
            expect (q.tuneToDetectedPitch(), "Tune to Pitch re-applies the analysis on request");
            expectEquals (getParam (q, ids::rootNote), 64.0f);
        }

        beginTest ("A sample without a clear pitch plays as recorded on C4 (MIDI 60)");
        {
            const auto noise = dir.getChildFile ("noise.wav");
            noise.deleteFile();
            {
                juce::AudioBuffer<float> buffer (1, 44100);
                juce::Random rng (42);
                for (int i = 0; i < buffer.getNumSamples(); ++i) buffer.setSample (0, i, rng.nextFloat() * 0.8f - 0.4f);
                juce::WavAudioFormat wavFormat;
                std::unique_ptr<juce::OutputStream> stream (noise.createOutputStream().release());
                auto writer = wavFormat.createWriterFor (stream, juce::AudioFormatWriterOptions{}.withSampleRate (44100.0)
                                                                                                .withNumChannels (1)
                                                                                                .withBitsPerSample (24));
                if (writer != nullptr) writer->writeFromAudioSampleBuffer (buffer, 0, buffer.getNumSamples());
            }
            LHSampleSynthProcessor p;
            setParam (p, ids::rootNote, 40.0f);
            setParam (p, ids::rootTune, 20.0f);
            expect (p.loadSampleSync (noise));
            expectEquals (getParam (p, ids::rootNote), 60.0f);
            expectWithinAbsoluteError (getParam (p, ids::rootTune), 0.0f, 0.01f);
        }

        beginTest ("LFO switch: new instances start with the LFO off; old projects that use the LFO keep it on");
        {
            LHSampleSynthProcessor fresh;
            expectEquals (getParam (fresh, ids::lfoOn), 0.0f);
            for (float amount : { 0.0f, 0.43f })
            {
                juce::MemoryBlock block;
                {
                    LHSampleSynthProcessor p;
                    setParam (p, ids::lfoToPitch, amount);
                    p.getStateInformation (block);
                }
                // Simulate a project saved before the switch existed.
                auto xml = juce::AudioProcessor::getXmlFromBinary (block.getData(), (int) block.getSize());
                for (auto* child = xml->getFirstChildElement(); child != nullptr; child = child->getNextElement())
                    if (child->getStringAttribute ("id") == ids::lfoOn) { xml->removeChildElement (child, true); break; }
                juce::MemoryBlock old;
                juce::AudioProcessor::copyXmlToBinary (*xml, old);
                LHSampleSynthProcessor q;
                q.setStateInformation (old.getData(), (int) old.getSize());
                expectEquals (getParam (q, ids::lfoOn), amount > 0.0f ? 1.0f : 0.0f, "LFO > Pitch " + juce::String (amount));
            }
        }

        beginTest ("FLAC files load (16 and 24 bit, upper-case extension)");
        {
            for (int bits : { 16, 24 })
            {
                const auto flac = dir.getChildFile ("Take_" + juce::String (bits) + ".FLAC");
                flac.deleteFile();
                {
                    auto buffer = test::makeSine (330.0, 0.5, 96000.0, 2);
                    juce::FlacAudioFormat format;
                    std::unique_ptr<juce::OutputStream> stream (flac.createOutputStream().release());
                    auto writer = format.createWriterFor (stream, juce::AudioFormatWriterOptions{}
                                                                     .withSampleRate (96000.0)
                                                                     .withNumChannels (2)
                                                                     .withBitsPerSample (bits));
                    expect (writer != nullptr);
                    if (writer != nullptr) writer->writeFromAudioSampleBuffer (buffer, 0, buffer.getNumSamples());
                }
                const auto result = lhss::SampleLoader::loadFile (flac);
                expect (result.ok(), result.error);
                if (result.ok())
                {
                    expectEquals (result.sample->getNumFrames(), 48000);
                    expectEquals (result.sample->getSampleRate(), 96000.0);
                }
            }
            const auto wildcard = lhss::SampleLoader::supportedWildcard();
            expect (juce::File ("x.FLAC").getFileName().matchesWildcard ("*.FLAC", false));
            expect (wildcard.contains ("*.flac") && wildcard.contains ("*.FLAC") && wildcard.contains ("*.Flac"));
        }

        beginTest ("Garbage state data is ignored");
        {
            LHSampleSynthProcessor p;
            const char junk[] = "not a plugin state";
            p.setStateInformation (junk, (int) sizeof (junk));
            expectEquals (getParam (p, ids::rootNote), 60.0f);
        }

        dir.deleteRecursively();
    }
};

static StateTests stateTests;
