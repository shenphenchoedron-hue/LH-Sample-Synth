#include <juce_events/juce_events.h>
#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "PluginEditor.h"
#include "PluginProcessor.h"

/** `LHSampleSynthTests --snapshot out.png [sample.wav]` renders the editor to a PNG
    (used to check the GUI layout without a host). */
static int renderSnapshot (const juce::String& outPath, const juce::String& samplePath)
{
    LHSampleSynthProcessor processor;
    if (samplePath.isNotEmpty()) processor.loadSampleSync (juce::File (samplePath));
    if (auto* p = processor.getAPVTS().getParameter ("loopOn")) p->setValueNotifyingHost (1.0f);
    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
    editor->setSize (LHSampleSynthEditor::kDesignWidth, LHSampleSynthEditor::kDesignHeight);
    const auto image = editor->createComponentSnapshot (editor->getLocalBounds());
    juce::File out (outPath);
    out.deleteFile();
    juce::FileOutputStream stream (out);
    juce::PNGImageFormat png;
    return png.writeImageToStream (image, stream) ? 0 : 1;
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI juceInit; // message manager for ChangeBroadcaster/Timer in the processor

    if (argc >= 3 && juce::String (argv[1]) == "--snapshot")
        return renderSnapshot (argv[2], argc >= 4 ? juce::String (argv[3]) : juce::String());

    juce::UnitTestRunner runner;
    runner.setAssertOnFailure (false);
    runner.runTestsInCategory ("LHSampleSynth");

    int failures = 0, passes = 0;
    for (int i = 0; i < runner.getNumResults(); ++i)
    {
        const auto* r = runner.getResult (i);
        failures += r->failures;
        passes += r->passes;
    }
    std::printf ("\n=== LH Sample Synth tests: %d checks passed, %d failed ===\n", passes, failures);
    return failures == 0 ? 0 : 1;
}
