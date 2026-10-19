#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "ManualView.h"
#include "ui/RackView.h"
#include "ui/Pages.h"
#include "ui/KeyboardStrip.h"

class ATFS1Editor : public juce::AudioProcessorEditor, public juce::FileDragAndDropTarget, private juce::Timer
{
public:
    explicit ATFS1Editor (ATFS1Processor&);
    ~ATFS1Editor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void filesDropped (const juce::StringArray& files, int, int) override;

    enum PageId { PPlay = 0, PVoice, POperator, PCommon, PFseq, PPerform, PEffects, PUtility, NumPages };
    void showPage (int p);

private:
    ATFS1Processor& proc;
    dh::fs::ui::LookAndFeelFs lnf;
    struct Backdrop : juce::Component { std::function<void (juce::Graphics&)> painter; void paint (juce::Graphics& g) override { if (painter) painter (g); } };
    Backdrop content;
    dh::fs::ui::RackView rack;
    dh::fs::ui::PlayPage play;
    dh::fs::ui::VoicePage voicePg;
    dh::fs::ui::OperatorPage opPg;
    dh::fs::ui::CommonPage commonPg;
    dh::fs::ui::FseqPage fseqPg;
    dh::fs::ui::PerformPage perfPg;
    dh::fs::ui::EffectsPage fxPg;
    dh::fs::ui::UtilityPage utilPg;
    dh::fs::ui::KeyboardStrip keys;
    std::array<juce::TextButton, NumPages> tabs;
    std::array<dh::fs::ui::Page*, NumPages> pages {};
    juce::TooltipWindow tips { this, 600 };
    int page = PPlay, cursor = 0;
    juce::String help, message;
    juce::uint32 messageUntil = 0;
    float cpuShown = -1.0f; int voicesShown = -1;
    juce::File dropHover;

    juce::TextButton manualBtn { "Manual" };
    dh::manual::ManualHost manualHost;     // el manual encima del editor
    static constexpr int baseW = 1600, baseH = 990;
    int rackH = 148, tabY = 148, pageY = 186, statusY = 910, keysY = 934;

    void onRackButton (const juce::String& id);
    void onDataKnob (int knob, int steps);
    void moveCursor (int d);
    void refreshAll();
    juce::String lcdLine1() const;
    juce::String lcdLine2() const;
    void timerCallback() override;
    void paintBackdrop (juce::Graphics&);
};
