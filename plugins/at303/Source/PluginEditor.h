#pragma once
// Editor del AT303. Arriba, el instrumento renderizado (grafito, sin marcas) con todo lo que se mueve dibujado
// encima: tapas de perilla rotadas, pads del paso, teclas y botones apretados, el display ámbar y la barra de carga
// del acento. Con "Inspect" el render se funde en el circuito en vivo (la escalera con sus capacitores, C13, las
// envolventes). Abajo, el panel de software: patrones, el editor de las dos listas del original (tiempos y
// alturas, con qué altura usa cada paso), el generador, la página extendida y la salida.

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "ManualView.h"

class At303Editor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit At303Editor (At303Processor&);
    ~At303Editor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

    static constexpr int baseW = 1260, instH = 702, baseH = 1050;

private:
    struct Content : public juce::Component
    {
        std::function<void (juce::Graphics&)> painter;
        void paint (juce::Graphics& g) override { if (painter) painter (g); }
    };
    class Instrument; class LaneGrid;
    using SA = juce::AudioProcessorValueTreeState::SliderAttachment;
    using BA = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using CA = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    struct Control
    {
        juce::String id, title; int kind = 0;
        std::unique_ptr<juce::Slider> s; std::unique_ptr<juce::ComboBox> c; std::unique_ptr<juce::TextButton> b;
        std::unique_ptr<SA> sa; std::unique_ptr<CA> ca; std::unique_ptr<BA> ba;
    };

    At303Processor& proc;
    struct Look; std::unique_ptr<Look> lnf;
    Content content;
    std::unique_ptr<Instrument> inst;
    std::unique_ptr<LaneGrid> grid;
    juce::OwnedArray<juce::TextButton> groupBtns, abBtns, numBtns, tabBtns;
    juce::TextButton prevBtn { "<" }, nextBtn { ">" }, presetBtn { "Init" }, manualBtn { "Manual" }, inspectBtn { "Inspect" };
    juce::TextButton modeBtn { "MIDI mode" }, syncBtn { "Follow host" }, tripBtn { "Triplets" }, lenDown { "-" }, lenUp { "+" }, genBtn { "Generate" }, mutBtn { "Mutate" }, stockBtn { "Stock 303" };
    std::unique_ptr<BA> syncAtt;
    std::vector<std::unique_ptr<Control>> detail;
    juce::String tab { "generate" };
    std::unique_ptr<juce::Component> manual;
    juce::TooltipWindow tips { this, 700 };
    juce::Rectangle<int> panelR, detailR, gridR, patR;
    int lastPat = -1;

    void buildDetail();
    void layoutDetail();
    void showPresetMenu();
    void showManual (bool on);
    void paintPanel (juce::Graphics& g);
    void timerCallback() override;
    void editPattern (std::function<void (dh::tb::Pattern&)> f);
    juce::String valueText (const juce::String& id) const;
};
