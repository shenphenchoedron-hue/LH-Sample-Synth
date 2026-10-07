#include <juce_audio_formats/juce_audio_formats.h>

#include "PluginProcessor.h"
#include "TestHelpers.h"
#include "parameters/ParameterIDs.h"

using namespace lhss;

namespace
{
juce::File writeTestWav (const juce::File& file, double sr, int channels, double seconds)
{
    file.deleteFile();
    auto buffer = test::makeSine (440.0, seconds, sr, channels);
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
            expectEquals (p.getAPVTS().getParameter (ids::rootNote)->getCurrentValueAsText(), juce::String ("C-2"));
            setParam (p, ids::rootNote, 60.0f);
            expectEquals (p.getAPVTS().getParameter (ids::rootNote)->getCurrentValueAsText(), juce::String ("C3"));
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
                if (std::abs (param->getValue() - param->getDefaultValue()) > 1.0e-6f) ++wrong;
            expectEquals (wrong, 0);
            expect (p.getLoadedSample() != nullptr);
            expect (p.getSampleStatus().state == LHSampleSynthProcessor::SampleState::Loaded);
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
