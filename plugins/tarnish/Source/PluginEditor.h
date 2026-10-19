#pragma once
// Editor de Tarnish: la máquina vista desde arriba (lo que se escucha, dibujado), la fila de máquinas con su
// variante, ocho perillas al frente y, debajo, pestañas con el resto (daño, tono, crush, velocidad, lo propio de
// cada máquina y más). Barra superior: presets, el botón de stop y el manual.

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "ui/LookTn.h"
#include "ui/DeckView.h"
#include "ManualView.h"

class TarnishEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit TarnishEditor (TarnishProcessor&);
    ~TarnishEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override { help.clear(); content.repaint (helpR); }

    static constexpr int baseW = 1120, baseH = 790, barH = 50;

private:
    struct Content : public juce::Component
    {
        std::function<void (juce::Graphics&)> painter;
        void paint (juce::Graphics& g) override { if (painter) painter (g); }
    };
    using SA = juce::AudioProcessorValueTreeState::SliderAttachment;
    using BA = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using CA = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    struct Knob { juce::Slider s; std::unique_ptr<SA> att; juce::String id, title; };
    struct Control
    {
        juce::String id, title; int kind = 0;             // 0 perilla, 1 lista, 2 interruptor
        std::unique_ptr<juce::Slider> s; std::unique_ptr<juce::ComboBox> c; std::unique_ptr<juce::TextButton> b;
        std::unique_ptr<SA> sa; std::unique_ptr<CA> ca; std::unique_ptr<BA> ba;
    };

    TarnishProcessor& proc;
    dh::tn::ui::Look lnf;
    Content content;
    dh::tn::ui::DeckView deck;
    std::array<Knob, 8> knobs;
    juce::OwnedArray<juce::TextButton> machineBtns, tabBtns;
    juce::ComboBox variant; std::unique_ptr<CA> variantAtt; int variantFor = -1;
    juce::ComboBox eq; std::unique_ptr<CA> eqAtt;
    std::vector<std::unique_ptr<Control>> detail;
    juce::String tab { "damage" };
    juce::TextButton prevBtn { "<" }, nextBtn { ">" }, presetBtn { "Init" }, stopBtn { "Stop" }, manualBtn { "Manual" };
    std::unique_ptr<BA> stopAtt;
    std::unique_ptr<juce::Component> manual;
    juce::TooltipWindow tips { this, 700 };
    juce::String help;
    juce::Rectangle<int> helpR, knobsR, detailR, machineR;
    int lastMachine = -1;

    void setupKnob (Knob& k, const juce::String& id, const juce::String& title, const juce::String& tip);
    void buildDetail();
    void layoutDetail();
    void bindVariant();
    void showPresetMenu();
    void showManual (bool on);
    void paintBackdrop (juce::Graphics& g);
    void timerCallback() override;
    juce::String valueText (const juce::String& id) const;
    static juce::String tip (const juce::String& id);
};
