#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "ui/Pages.h"
#include "ManualView.h"

class MurmurEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit MurmurEditor (MurmurProcessor&);
    ~MurmurEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    void showPage (int p);          // 0 simple, 1 advanced
    void showManual (bool on);

private:
    struct Backdrop : juce::Component
    {
        std::function<void (juce::Graphics&)> painter;
        void paint (juce::Graphics& g) override { if (painter) painter (g); }
    };
    MurmurProcessor& proc;
    dh::mm::ui::LookAndFeelMm lnf;
    Backdrop content;
    dh::mm::ui::SimplePage simple;
    dh::mm::ui::AdvancedPage advanced;
    juce::TextButton prevBtn { "<" }, nextBtn { ">" }, presetBtn, saveBtn { "Save" }, simpleBtn { "Simple" }, advBtn { "Advanced" }, manualBtn { "Manual" };
    std::unique_ptr<juce::Component> manual;
    juce::String help;
    juce::Rectangle<int> statusR;
    int page = 0;

    static constexpr int baseW = 1240, baseH = 780, barH = 60, footH = 30;
    void paintBackdrop (juce::Graphics&);
    void timerCallback() override;
    void showPresetMenu();
    void savePreset();
    void refreshAll();
};
