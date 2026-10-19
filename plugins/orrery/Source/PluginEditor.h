#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "ManualView.h"
#include "ui/Pages.h"

class OrreryEditor : public juce::AudioProcessorEditor, public juce::DragAndDropContainer, private juce::Timer
{
public:
    explicit OrreryEditor (OrreryProcessor&);
    ~OrreryEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    void showPage (int p);      // 0 simple, 1 carriles, 2 armonía, 3 salidas

private:
    struct Backdrop : juce::Component
    {
        std::function<void (juce::Graphics&)> painter;
        std::function<void (const juce::MouseEvent&)> down, drag;
        void paint (juce::Graphics& g) override { if (painter) painter (g); }
        void mouseDown (const juce::MouseEvent& e) override { if (down) down (e); }
        void mouseDrag (const juce::MouseEvent& e) override { if (drag) drag (e); }
    };
    OrreryProcessor& proc;
    dh::orr::ui::LookAndFeelOr lnf;
    Backdrop content;
    dh::orr::ui::SimplePage simple;
    dh::orr::ui::LanesPage lanes;
    dh::orr::ui::HarmonyPage harmony;
    dh::orr::ui::OutputsPage outputs;
    juce::TextButton playBtn, prevBtn { "<" }, nextBtn { ">" }, presetBtn, saveBtn { "Save" }, captureBtn { "Capture MIDI" }, panicBtn { "All notes off" };
    juce::TextButton simpleBtn { "Simple" }, advBtn { "Advanced" }, tabLanes { "Lanes" }, tabHarm { "Harmony & session" }, tabOut { "Outputs" };
    juce::TooltipWindow tips { this, 600 };
    juce::String help, message;
    juce::uint32 messageUntil = 0;
    juce::File captured;
    juce::Rectangle<int> statusR, dragR, wordR;
    int page = 0;
    float lastPlay = -1.0f; juce::String lastStatus;

    juce::TextButton manualBtn { "Manual" };
    dh::manual::ManualHost manualHost;     // el manual encima del editor
    static constexpr int baseW = 1320, baseH = 820, barH = 64, footH = 30;
    void paintBackdrop (juce::Graphics&);
    void timerCallback() override;
    void showPresetMenu();
    void doCapture();
    void savePreset();
    juce::String statusText() const;
    void refreshAll();
    std::unique_ptr<juce::Timer> recorder;     // demos para la web (ORRERY_RECORD)
};
