// valencerecord: abre el editor de Valence sin audio y deja que VALENCE_RECORD grabe los cuadros de una demo
// (ver Source/ui/AtomRecorder.h). Sale cuando aparece done.txt (o error.txt) en la carpeta de salida.
#include <JuceHeader.h>
#include "PluginProcessor.h"

class RecordApp : public juce::JUCEApplication, private juce::Timer
{
public:
    const juce::String getApplicationName() override { return "valencerecord"; }
    const juce::String getApplicationVersion() override { return "1.0"; }
    void initialise (const juce::String&) override
    {
        out = juce::File (juce::SystemStats::getEnvironmentVariable ("VALENCE_RECORD", {}));
        if (out == juce::File() || juce::SystemStats::getEnvironmentVariable ("VALENCE_DEMO", {}).isEmpty())
        {
            std::printf ("usage: VALENCE_RECORD=outdir VALENCE_DEMO=demodir [VALENCE_RECORD_FROM=s VALENCE_RECORD_SECS=s VALENCE_RECORD_W=1280 VALENCE_RECORD_H=720] valencerecord\n");
            quit(); return;
        }
        proc = std::make_unique<ValenceProcessor>();
        editor.reset (proc->createEditor());
        editor->setTopLeftPosition (0, 0);
        editor->addToDesktop (juce::ComponentPeer::windowIsTemporary);
        editor->setVisible (true);
        startTimer (200);
    }
    void shutdown() override { stopTimer(); editor.reset(); proc.reset(); }
private:
    std::unique_ptr<ValenceProcessor> proc;
    std::unique_ptr<juce::AudioProcessorEditor> editor;
    juce::File out;
    void timerCallback() override
    {
        if (out.getChildFile ("done.txt").existsAsFile()) { std::printf ("frames: %s\n", out.getChildFile ("done.txt").loadFileAsString().toRawUTF8()); quit(); }
        else if (out.getChildFile ("error.txt").existsAsFile()) { std::printf ("error: %s\n", out.getChildFile ("error.txt").loadFileAsString().toRawUTF8()); quit(); }
    }
};

START_JUCE_APPLICATION (RecordApp)
