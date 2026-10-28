#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "ui/SimplePage.h"
#include "ui/AtomRecorder.h"

// Editor: barra (Play, sonido de prueba, presets, captura, pánico) y cuatro páginas: Simple (el átomo y las macros),
// Layers, Harmony y Outputs.
class ValenceEditor : public juce::AudioProcessorEditor, public juce::DragAndDropContainer, public dh::HelpSink, private juce::Timer
{
public:
    explicit ValenceEditor (ValenceProcessor&);
    ~ValenceEditor() override;
    void routeHelp (const juce::String& s) override { help = s; content.repaint (statusR); }
    void paint (juce::Graphics&) override;
    void resized() override;
    void showPage (int p);      // 0 Simple, 1 Layers, 2 Harmony, 3 Outputs

private:
    struct Backdrop : juce::Component
    {
        std::function<void (juce::Graphics&)> painter;
        std::function<void (const juce::MouseEvent&)> drag;
        void paint (juce::Graphics& g) override { if (painter) painter (g); }
        void mouseDrag (const juce::MouseEvent& e) override { if (drag) drag (e); }
    };
    ValenceProcessor& proc;
    dh::val::ui::LookAndFeelVal lnf;
    Backdrop content;
    dh::val::ui::SimplePage simple;
    dh::val::ui::LayersPage layers;
    dh::val::ui::HarmonyPage harmony;
    dh::val::ui::OutputsPage outputs;
    std::array<dh::val::ui::Page*, 4> pages { &simple, &layers, &harmony, &outputs };
    std::unique_ptr<dh::val::ui::AtomRecorder> recorder;     // VALENCE_RECORD
    dh::val::ui::PillToggle previewBtn;
    juce::TextButton playBtn, prevBtn { "<" }, nextBtn { ">" }, presetBtn, saveBtn { "Save" }, captureBtn { "Capture MIDI" }, panicBtn { "All notes off" };
    std::array<juce::TextButton, 4> tabs { juce::TextButton ("Simple"), juce::TextButton ("Layers"), juce::TextButton ("Harmony"), juce::TextButton ("Outputs") };
    juce::TooltipWindow tips { this, 600 };
    juce::String help, message;
    juce::uint32 messageUntil = 0;
    juce::File captured;
    juce::Rectangle<int> statusR, dragR;
    int page = 0;
    float lastPlay = -1.0f;
    static constexpr int baseW = 1320, baseH = 860, barH = 64, footH = 30;
    void paintBackdrop (juce::Graphics&);
    void timerCallback() override;
    void showPresetMenu();
    void doCapture();
    void savePreset();
};
