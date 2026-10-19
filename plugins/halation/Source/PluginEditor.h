#pragma once
// Editor de Halation: la sala arriba (lo que se escucha, dibujado), la perilla larga Delay ↔ Space, las cinco
// perillas principales, el carácter y el tipo de sala, la fila de módulos y debajo los ajustes del módulo elegido
// (o el panel More). Barra superior: presets, escenas A/B con morph y el manual.

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "ui/LookHa.h"
#include "ui/RoomView.h"
#include "ManualView.h"

class SpaceEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit SpaceEditor (SpaceProcessor&);
    ~SpaceEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override { help.clear(); content.repaint (helpR); }

    static constexpr int baseW = 1120, baseH = 820, barH = 50;

private:
    struct Content : public juce::Component
    {
        std::function<void (juce::Graphics&)> painter;
        void paint (juce::Graphics& g) override { if (painter) painter (g); }
    };
    using SA = juce::AudioProcessorValueTreeState::SliderAttachment;
    using BA = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using CA = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    struct Knob
    {
        juce::Slider s; std::unique_ptr<SA> att; juce::String id, title;
    };
    struct Control                      // un control del panel de detalle (perilla, lista o interruptor)
    {
        juce::String id, title; int kind = 0;             // 0 perilla, 1 lista, 2 interruptor
        std::unique_ptr<juce::Slider> s; std::unique_ptr<juce::ComboBox> c; std::unique_ptr<juce::TextButton> b;
        std::unique_ptr<SA> sa; std::unique_ptr<CA> ca; std::unique_ptr<BA> ba;
    };

    SpaceProcessor& proc;
    dh::sp::ui::Look lnf;
    Content content;
    dh::sp::ui::RoomView room;
    juce::Slider space; std::unique_ptr<SA> spaceAtt;
    Knob time, division, size, length, tone, mix;
    juce::TextButton syncBtn { "Sync" }, pingBtn { "Ping-pong" };
    std::unique_ptr<BA> syncAtt, pingAtt;
    juce::ComboBox pattern; std::unique_ptr<CA> patternAtt;
    juce::OwnedArray<juce::TextButton> charBtns, roomBtns, modBtns;
    juce::TextButton moreBtn { "More" };
    std::vector<std::unique_ptr<Control>> detail;
    juce::String selected { "shimmer" };
    juce::TextButton prevBtn { "<" }, nextBtn { ">" }, presetBtn { "Init" }, storeB { "Store B" }, manualBtn { "Manual" };
    juce::Slider morph; std::unique_ptr<SA> morphAtt;
    std::unique_ptr<juce::Component> manual;
    juce::TooltipWindow tips { this, 700 };
    juce::String help;
    juce::Rectangle<int> helpR, knobsR, detailR;

    void setupKnob (Knob& k, const juce::String& id, const juce::String& title, const juce::String& tip);
    void buildDetail();
    void layoutDetail();
    void showPresetMenu();
    void showManual (bool on);
    void paintBackdrop (juce::Graphics& g);
    void timerCallback() override;
    juce::String valueText (const juce::String& id) const;
    static juce::String moduleTip (const juce::String& m);
    static juce::String moduleTitle (const juce::String& m);
};
